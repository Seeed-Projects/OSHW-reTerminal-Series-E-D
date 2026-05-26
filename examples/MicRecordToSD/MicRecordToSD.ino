/*
 * MicRecordToSD — PDM Microphone Recording to SD Card
 *
 * Compatible with : reTerminal E1001 / E1002 / E1003
 * NOT compatible  : reTerminal E1004 (no onboard microphone)
 *
 * How to use
 * ----------
 * 1. Select your device model in the USER CONFIGURATION section below.
 * 2. Insert a FAT32-formatted MicroSD card (≤ 64 GB).
 * 3. Flash to your device (board: XIAO_ESP32S3, PSRAM: OPI PSRAM, Flash: 8 MB).
 * 4. Open a serial monitor on the carrier USB-UART bridge (GPIO43 TX / GPIO44 RX,
 *    115200 baud) — this is Serial1, not the USB-CDC Serial.
 * 5. Press KEY0 (right button) to start recording.
 *    The LED blinks while recording is in progress.
 * 6. Press KEY0 again (or wait for MAX_RECORD_SECS) to stop.
 *    The WAV file is saved to RECORD_DIR on the SD card.
 *
 * Output file format
 * ------------------
 * Uncompressed PCM WAV, 16-bit, mono, SAMPLE_RATE Hz.
 * Files are named REC_0001.WAV, REC_0002.WAV, … (auto-incremented).
 *
 * Required libraries
 * ------------------
 * All built-in — no Library Manager installs needed:
 *   SD.h / SPI.h          (SD card)
 *   driver/i2s_pdm.h      (ESP-IDF 5.x new PDM-RX driver, Arduino ESP32 ≥ 3.0)
 *   driver/i2s_common.h   (channel handle type)
 */

// ============================================================
// USER CONFIGURATION
// ============================================================

// Uncomment ONE line to select your hardware model:
// #define DEVICE_E1001_E1002   // reTerminal E1001 or E1002
#define DEVICE_E1003      // reTerminal E1003

// Recording parameters
#define SAMPLE_RATE      16000U  // Sample rate in Hz (8000 / 16000 / 44100)
#define MAX_RECORD_SECS  30      // Auto-stop after this many seconds (0 = unlimited)
#define RECORD_DIR       "/REC"  // Directory on the SD card root

// ============================================================
// END OF USER CONFIGURATION — no need to edit below this line
// ============================================================

#include <SD.h>
#include <SPI.h>
#include <driver/i2s_pdm.h>    // ESP-IDF 5.x PDM-RX API (Arduino ESP32 >= 3.0)
#include <driver/i2s_common.h>

// ---------- Serial debug ---------
#define PIN_SERIAL_RX   44
#define PIN_SERIAL_TX   43
#define LOG             Serial1

// ---------- PDM Microphone -------
// Same on E1001, E1002, and E1003.
#define PIN_MIC_CLK     42   // GPIO42 — PDM_CLK  (R109 in schematic)
#define PIN_MIC_DATA    41   // GPIO41 — PDM_DATA (R110 in schematic)
#define PIN_MIC_PWR_EN  38   // GPIO38 — MIC power enable (TPS22916CYFPR EN, ESP_IO3B)

// ---------- SD Card --------------
// SPI bus is shared with the ePaper display; a separate CS keeps them independent.
#define PIN_SD_DET       15  // Card detect (LOW = card present)
#define PIN_SD_CS        14  // SPI Chip Select
#define PIN_SD_MISO       8
#define PIN_SD_MOSI       9
#define PIN_SD_SCK        7

// ---------- User Button ----------
#define PIN_BTN_KEY0      3  // KEY0 — active LOW (hardware pull-up)

// ---------- Model-specific pins --
#if defined(DEVICE_E1001_E1002)
  #define PIN_SD_EN    16    // GPIO16 — SD card power enable
  #define PIN_LED       6    // GPIO6  — onboard LED (inverted: LOW = ON)
#elif defined(DEVICE_E1003)
  #define PIN_SD_EN    39    // GPIO39 — SD card power enable
  #define PIN_LED      16    // GPIO16 — onboard LED (inverted: LOW = ON)
#else
  #error "Please define DEVICE_E1001_E1002 or DEVICE_E1003 in the USER CONFIGURATION section."
#endif

// ---------- I2S / Audio ----------
#define I2S_PORT          I2S_NUM_0
#define DMA_BUF_COUNT     8     // number of DMA descriptors
#define DMA_BUF_LEN       512   // frames per DMA descriptor
#define BITS_PER_SAMPLE   16
#define AUDIO_CHANNELS    1
#define BYTES_PER_SAMPLE  (BITS_PER_SAMPLE / 8)
#define BYTES_PER_SEC     (SAMPLE_RATE * AUDIO_CHANNELS * BYTES_PER_SAMPLE)

// Single-read chunk: matches one DMA buffer (512 frames × 2 bytes = 1024 bytes)
#define READ_BUF_BYTES    (DMA_BUF_LEN * BYTES_PER_SAMPLE)
static uint8_t s_dmaBuf[READ_BUF_BYTES];

// ============================================================
// WAV file header (44 bytes, little-endian)
// ============================================================
#pragma pack(push, 1)
struct WavHeader {
    // RIFF chunk
    char     riffTag[4];      // "RIFF"
    uint32_t riffSize;        // file size − 8
    char     waveTag[4];      // "WAVE"
    // fmt sub-chunk
    char     fmtTag[4];       // "fmt "
    uint32_t fmtSize;         // 16 for PCM
    uint16_t audioFormat;     // 1 = PCM
    uint16_t numChannels;     // 1 = mono
    uint32_t sampleRate;
    uint32_t byteRate;        // sampleRate × channels × bytesPerSample
    uint16_t blockAlign;      // channels × bytesPerSample
    uint16_t bitsPerSample;
    // data sub-chunk
    char     dataTag[4];      // "data"
    uint32_t dataSize;        // audio payload in bytes
};
#pragma pack(pop)

static_assert(sizeof(WavHeader) == 44, "WavHeader must be 44 bytes");

// ============================================================
// Global state
// ============================================================
static i2s_chan_handle_t s_rxHandle    = nullptr;  // ESP-IDF 5.x channel handle
static SPIClass          s_spiSD(HSPI);
static File        s_wavFile;
static bool        s_recording     = false;
static uint32_t    s_recordedBytes = 0;
static uint32_t    s_fileIndex     = 1;

// Button debounce
static bool          s_lastRawBtn    = HIGH;
static bool          s_stableBtn     = HIGH;
static unsigned long s_debounceMs    = 0;
static const unsigned long DEBOUNCE_DELAY = 50;

// LED blink
static unsigned long s_lastBlinkMs  = 0;
static bool          s_ledState     = false;

// ============================================================
// LED helpers (inverted logic)
// ============================================================
static void ledOn()  { digitalWrite(PIN_LED, LOW);  }
static void ledOff() { digitalWrite(PIN_LED, HIGH); }

// ============================================================
// WAV helpers
// ============================================================
static void writeWavHeader(File& f, uint32_t dataBytes)
{
    WavHeader h;
    memcpy(h.riffTag,  "RIFF", 4);
    h.riffSize     = 36 + dataBytes;
    memcpy(h.waveTag,  "WAVE", 4);
    memcpy(h.fmtTag,   "fmt ", 4);
    h.fmtSize      = 16;
    h.audioFormat  = 1;
    h.numChannels  = AUDIO_CHANNELS;
    h.sampleRate   = SAMPLE_RATE;
    h.byteRate     = BYTES_PER_SEC;
    h.blockAlign   = AUDIO_CHANNELS * BYTES_PER_SAMPLE;
    h.bitsPerSample = BITS_PER_SAMPLE;
    memcpy(h.dataTag,  "data", 4);
    h.dataSize     = dataBytes;
    f.write(reinterpret_cast<const uint8_t*>(&h), sizeof(h));
}

// ============================================================
// SD card helpers
// ============================================================
static bool mountSD()
{
    pinMode(PIN_SD_EN, OUTPUT);
    digitalWrite(PIN_SD_EN, HIGH);
    delay(10);

    s_spiSD.end();
    s_spiSD.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

    if (!SD.begin(PIN_SD_CS, s_spiSD)) {
        LOG.println("[SD] Initialization failed — check card and formatting (FAT32).");
        return false;
    }
    LOG.printf("[SD] Mounted. Type: %s  Size: %llu MB\n",
               SD.cardType() == CARD_SDHC ? "SDHC" : "SD",
               SD.cardSize() / (1024ULL * 1024ULL));
    return true;
}

static String nextFilename()
{
    if (!SD.exists(RECORD_DIR)) {
        SD.mkdir(RECORD_DIR);
    }
    char buf[32];
    while (true) {
        snprintf(buf, sizeof(buf), "%s/REC_%04u.WAV", RECORD_DIR, s_fileIndex);
        if (!SD.exists(buf)) break;
        s_fileIndex++;
        if (s_fileIndex > 9999) s_fileIndex = 1;
    }
    return String(buf);
}

// ============================================================
// PDM / I2S helpers  (ESP-IDF 5.x new-API, Arduino ESP32 >= 3.0)
// ============================================================
static bool initMic()
{
    // ── Step 1: power up the microphone via the load switch ──
    LOG.println("[MIC] Powering on microphone...");
    pinMode(PIN_MIC_PWR_EN, OUTPUT);
    digitalWrite(PIN_MIC_PWR_EN, HIGH);
    delay(50);  // give the LDO and PDM decimation filter time to start up

    // ── Step 2: create an I2S RX channel ──
    LOG.println("[MIC] Creating I2S channel...");
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.dma_desc_num  = DMA_BUF_COUNT;
    chanCfg.dma_frame_num = DMA_BUF_LEN;
    chanCfg.auto_clear    = true;

    esp_err_t err = i2s_new_channel(&chanCfg, nullptr, &s_rxHandle);
    if (err != ESP_OK) {
        LOG.printf("[MIC] i2s_new_channel failed: 0x%x\n", err);
        return false;
    }

    // ── Step 3: configure PDM-RX mode ──
    LOG.println("[MIC] Configuring PDM-RX mode...");
    i2s_pdm_rx_config_t pdmCfg = {};
    pdmCfg.clk_cfg  = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE);
    pdmCfg.slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                      I2S_SLOT_MODE_MONO);
    pdmCfg.gpio_cfg.clk             = static_cast<gpio_num_t>(PIN_MIC_CLK);
    pdmCfg.gpio_cfg.din             = static_cast<gpio_num_t>(PIN_MIC_DATA);
    pdmCfg.gpio_cfg.invert_flags.clk_inv = false;

    err = i2s_channel_init_pdm_rx_mode(s_rxHandle, &pdmCfg);
    if (err != ESP_OK) {
        LOG.printf("[MIC] i2s_channel_init_pdm_rx_mode failed: 0x%x\n", err);
        i2s_del_channel(s_rxHandle);
        s_rxHandle = nullptr;
        return false;
    }

    // ── Step 4: enable (starts the clock and DMA) ──
    LOG.println("[MIC] Enabling channel...");
    err = i2s_channel_enable(s_rxHandle);
    if (err != ESP_OK) {
        LOG.printf("[MIC] i2s_channel_enable failed: 0x%x\n", err);
        i2s_del_channel(s_rxHandle);
        s_rxHandle = nullptr;
        return false;
    }

    // ── Step 5: warm-up — discard a few DMA buffers with a finite timeout ──
    // The PDM decimation filter needs several milliseconds to settle.
    // Using a 500 ms timeout instead of portMAX_DELAY prevents hanging if
    // the hardware is not producing data for any reason.
    LOG.println("[MIC] Warming up PDM filter...");
    size_t dummy;
    for (int i = 0; i < 3; i++) {
        i2s_channel_read(s_rxHandle, s_dmaBuf, sizeof(s_dmaBuf),
                         &dummy, pdMS_TO_TICKS(500));
    }

    LOG.printf("[MIC] PDM initialized. Rate=%u Hz  Bits=%d  CLK=GPIO%d  DATA=GPIO%d\n",
               SAMPLE_RATE, BITS_PER_SAMPLE, PIN_MIC_CLK, PIN_MIC_DATA);
    return true;
}

// ============================================================
// Recording control
// ============================================================
static bool startRecording()
{
    String fname = nextFilename();
    s_wavFile = SD.open(fname, FILE_WRITE);
    if (!s_wavFile) {
        LOG.printf("[REC] Cannot create file: %s\n", fname.c_str());
        return false;
    }
    writeWavHeader(s_wavFile, 0);   // placeholder — filled in when recording stops
    s_recordedBytes = 0;
    s_recording     = true;
    ledOn();
    LOG.printf("[REC] Recording started → %s\n", fname.c_str());
    return true;
}

static void stopRecording()
{
    s_recording = false;
    ledOff();

    // Seek back to the beginning and rewrite the header with the real data size.
    s_wavFile.seek(0);
    writeWavHeader(s_wavFile, s_recordedBytes);
    s_wavFile.close();

    float seconds = static_cast<float>(s_recordedBytes) / BYTES_PER_SEC;
    LOG.printf("[REC] Recording stopped. %u bytes saved (%.1f s).\n",
               s_recordedBytes, seconds);
    s_fileIndex++;
}

// ============================================================
// setup()
// ============================================================
void setup()
{
    LOG.begin(115200, SERIAL_8N1, PIN_SERIAL_RX, PIN_SERIAL_TX);
    delay(500);   // brief pause for the serial bridge to enumerate

    LOG.println("=========================================");
    LOG.println("  MicRecordToSD — reTerminal E Series");
#if defined(DEVICE_E1001_E1002)
    LOG.println("  Device: E1001 / E1002");
#else
    LOG.println("  Device: E1003");
#endif
    LOG.println("=========================================");

    // LED
    pinMode(PIN_LED, OUTPUT);
    ledOff();

    // Startup blink to confirm power-on
    for (int i = 0; i < 3; i++) {
        ledOn();  delay(100);
        ledOff(); delay(100);
    }

    // User button (hardware pull-up, active LOW)
    pinMode(PIN_BTN_KEY0, INPUT);

    // SD card
    LOG.println("[SD] Mounting...");
    if (!mountSD()) {
        LOG.println("[SD] FATAL: could not mount SD card. Halting.");
        while (true) { delay(1000); }
    }

    // PDM microphone
    LOG.println("[MIC] Initializing PDM microphone...");
    if (!initMic()) {
        LOG.println("[MIC] FATAL: microphone init failed. Halting.");
        while (true) { delay(1000); }
    }

    LOG.println();
    LOG.printf("[READY] Press KEY0 to start recording (max %d s).\n", MAX_RECORD_SECS);
    LOG.printf("[READY] Files will be saved to %s/REC_XXXX.WAV\n", RECORD_DIR);
}

// ============================================================
// loop()
// ============================================================
void loop()
{
    // -------------------------------------------------------
    // Debounced button handling
    // -------------------------------------------------------
    const bool rawBtn = digitalRead(PIN_BTN_KEY0);
    if (rawBtn != s_lastRawBtn) {
        s_debounceMs = millis();
        s_lastRawBtn = rawBtn;
    }
    if ((millis() - s_debounceMs) > DEBOUNCE_DELAY && rawBtn != s_stableBtn) {
        s_stableBtn = rawBtn;
        if (s_stableBtn == LOW) {   // falling edge = button pressed
            if (!s_recording) {
                startRecording();
            } else {
                stopRecording();
            }
        }
    }

    // -------------------------------------------------------
    // Audio capture (only while recording)
    // -------------------------------------------------------
    if (s_recording) {
        size_t bytesRead = 0;
        // Use a 200 ms timeout so the button check in the next loop iteration
        // is still reached even if the DMA is unexpectedly slow.
        const esp_err_t err = i2s_channel_read(s_rxHandle, s_dmaBuf, sizeof(s_dmaBuf),
                                               &bytesRead, pdMS_TO_TICKS(200));
        if (err == ESP_OK && bytesRead > 0) {
            s_wavFile.write(s_dmaBuf, bytesRead);
            s_recordedBytes += bytesRead;
        } else if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            LOG.printf("[REC] i2s_channel_read error: 0x%x — stopping.\n", err);
            stopRecording();
            return;
        }

        // Auto-stop when MAX_RECORD_SECS is reached
        if (MAX_RECORD_SECS > 0 && s_recordedBytes >= static_cast<uint32_t>(BYTES_PER_SEC) * MAX_RECORD_SECS) {
            LOG.println("[REC] Maximum duration reached — stopping automatically.");
            stopRecording();
            return;
        }

        // LED blink at 500 ms period while recording
        const unsigned long now = millis();
        if (now - s_lastBlinkMs >= 500) {
            s_lastBlinkMs = now;
            s_ledState    = !s_ledState;
            if (s_ledState) ledOn(); else ledOff();
        }
    }
}
