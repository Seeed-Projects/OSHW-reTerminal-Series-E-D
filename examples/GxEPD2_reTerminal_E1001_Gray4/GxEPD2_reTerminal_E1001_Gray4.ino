#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

// ===== Pin mapping (E1001) =====
#define EPD_SCK_PIN   7
#define EPD_MOSI_PIN  9
#define EPD_CS_PIN    10
#define EPD_DC_PIN    11
#define EPD_RES_PIN   12
#define EPD_BUSY_PIN  13

#define EPD_W  800
#define EPD_H  480

SPIClass hspi(HSPI);
SPISettings spiSet(2000000, MSBFIRST, SPI_MODE0);

// Gray levels
#define G_BLACK       0
#define G_DARK_GRAY   1
#define G_LIGHT_GRAY  2
#define G_WHITE       3

// UC8179 gray LUT tables (verbatim from Seeed_GFX UC8179_Defines.h)
// Each LUT is 7 phases x 6 bytes = 42 bytes.
static const uint8_t LUT_VCOM_GRAY[] = {
  0x00,0x00,0x06,0x08,0x07,0x01,
  0x00,0x06,0x0A,0x0B,0x0A,0x01,
  0x00,0x03,0x03,0x00,0x00,0x03,
  0x00,0x05,0x09,0x06,0x06,0x01,
  0x00,0x02,0x02,0x0A,0x0A,0x01,
  0x00,0x0A,0x11,0x06,0x07,0x01,
  0x00,0x02,0x01,0x02,0x01,0x01,
};

static const uint8_t LUT_WW_GRAY[] = {
  0x15,0x00,0x06,0x08,0x07,0x01,
  0x54,0x06,0x0A,0x0B,0x0A,0x01,
  0x90,0x03,0x03,0x00,0x00,0x03,
  0x2A,0x05,0x09,0x06,0x06,0x01,
  0xAA,0x02,0x02,0x0A,0x0A,0x01,
  0x00,0x0A,0x11,0x06,0x07,0x01,
  0x28,0x02,0x01,0x02,0x01,0x01,
};

static const uint8_t LUT_KW_GRAY[] = {
  0x2A,0x00,0x06,0x08,0x07,0x01,
  0x59,0x06,0x0A,0x0B,0x0A,0x01,
  0x90,0x03,0x03,0x00,0x00,0x03,
  0x5A,0x05,0x09,0x06,0x06,0x01,
  0xA8,0x02,0x02,0x0A,0x0A,0x01,
  0x45,0x0A,0x11,0x06,0x07,0x01,
  0xA8,0x02,0x01,0x02,0x01,0x01,
};

static const uint8_t LUT_WK_GRAY[] = {
  0x16,0x00,0x06,0x08,0x07,0x01,
  0xA0,0x06,0x0A,0x0B,0x0A,0x01,
  0x90,0x03,0x03,0x00,0x00,0x03,
  0x99,0x05,0x09,0x06,0x06,0x01,
  0xA0,0x02,0x02,0x0A,0x0A,0x01,
  0x40,0x0A,0x11,0x06,0x07,0x01,
  0x20,0x02,0x01,0x02,0x01,0x01,
};

static const uint8_t LUT_KK_GRAY[] = {
  0x26,0x00,0x06,0x08,0x07,0x01,
  0x6A,0x06,0x0A,0x0B,0x0A,0x01,
  0x90,0x03,0x03,0x00,0x00,0x03,
  0x65,0x05,0x09,0x06,0x06,0x01,
  0x50,0x02,0x02,0x0A,0x0A,0x01,
  0x10,0x0A,0x11,0x06,0x07,0x01,
  0x10,0x02,0x01,0x02,0x01,0x01,
};

static const uint8_t CMD_USER_GRAY[] = {
  0x17, 0x3F, 0x3F, 0x07, 0x06, 0x12,
};

// ============================================================
// 4-level grayscale canvas (2bpp)
// ============================================================
class Gray4Canvas : public Adafruit_GFX
{
public:
  Gray4Canvas(uint16_t w, uint16_t h) : Adafruit_GFX(w, h), _buf(nullptr) {}

  bool begin()
  {
    uint32_t sz = uint32_t(_width) * _height / 4;
    _buf = (uint8_t*)malloc(sz);
    if (_buf) memset(_buf, 0xFF, sz); // fill white (0b11 = 3 = white)
    return _buf != nullptr;
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override
  {
    if (!_buf) return;
    if (x < 0 || x >= width() || y < 0 || y >= height()) return;

    int16_t rx = x, ry = y;
    switch (getRotation()) {
      case 1: rx = _width - 1 - y; ry = x; break;
      case 2: rx = _width - 1 - x; ry = _height - 1 - y; break;
      case 3: rx = y; ry = _height - 1 - x; break;
    }

    uint8_t g = color & 0x03;
    uint32_t idx = uint32_t(ry) * (_width / 4) + rx / 4;
    uint8_t shift = (3 - (rx & 3)) * 2;
    _buf[idx] = (_buf[idx] & ~(0x03 << shift)) | (g << shift);
  }

  void fillScreen(uint16_t color) override
  {
    if (!_buf) return;
    uint8_t g = color & 0x03;
    uint8_t fill = (g << 6) | (g << 4) | (g << 2) | g;
    memset(_buf, fill, uint32_t(_width) * _height / 4);
  }

  uint8_t getPixel(int16_t x, int16_t y) const
  {
    if (!_buf || x < 0 || x >= _width || y < 0 || y >= _height) return 0;
    uint32_t idx = uint32_t(y) * (_width / 4) + x / 4;
    uint8_t shift = (3 - (x & 3)) * 2;
    return (_buf[idx] >> shift) & 0x03;
  }

  uint8_t* buffer() { return _buf; }

private:
  uint8_t* _buf;
};

Gray4Canvas canvas(EPD_W, EPD_H);

// ============================================================
// UC8179 SPI helpers
// ============================================================
void checkBusy() {
  delay(10);
  while (!digitalRead(EPD_BUSY_PIN)) delay(10);
}

void writeCommand(uint8_t cmd) {
  hspi.beginTransaction(spiSet);
  digitalWrite(EPD_DC_PIN, LOW);
  digitalWrite(EPD_CS_PIN, LOW);
  hspi.transfer(cmd);
  digitalWrite(EPD_CS_PIN, HIGH);
  digitalWrite(EPD_DC_PIN, HIGH);
  hspi.endTransaction();
}

void writeData(uint8_t data) {
  hspi.beginTransaction(spiSet);
  digitalWrite(EPD_CS_PIN, LOW);
  hspi.transfer(data);
  digitalWrite(EPD_CS_PIN, HIGH);
  hspi.endTransaction();
}

void writeLUT(uint8_t cmd, const uint8_t* lut, uint16_t len) {
  writeCommand(cmd);
  for (uint16_t i = 0; i < len; i++) writeData(lut[i]);
}

// ============================================================
// UC8179 gray mode initialization
// ============================================================
void initGrayMode()
{
  // Hardware reset
  digitalWrite(EPD_RES_PIN, LOW); delay(10);
  digitalWrite(EPD_RES_PIN, HIGH); delay(10);
  checkBusy();

  // Power setting (0x01)
  writeCommand(0x01);
  writeData(0x07);
  writeData(CMD_USER_GRAY[0]);
  writeData(CMD_USER_GRAY[1]);
  writeData(CMD_USER_GRAY[2]);
  writeData(CMD_USER_GRAY[3]);

  // PLL (0x30)
  writeCommand(0x30);
  writeData(CMD_USER_GRAY[4]);

  // VCOM DC (0x82)
  writeCommand(0x82);
  writeData(CMD_USER_GRAY[5]);

  // Booster (0x06)
  writeCommand(0x06);
  writeData(0x27);
  writeData(0x27);
  writeData(0x28);
  writeData(0x17);

  // Power ON (0x04)
  writeCommand(0x04);
  delay(100);
  checkBusy();

  // Panel Setting (0x00)
  writeCommand(0x00);
  writeData(0x3F);

  // Power saving (0xE3)
  writeCommand(0xE3);
  writeData(0x88);

  // VCOM and Data interval (0x50)
  writeCommand(0x50);
  writeData(0x10);
  writeData(0x07);

  // PLL setting (0x52)
  writeCommand(0x52);
  writeData(0x00);

  // Resolution (0x61)
  writeCommand(0x61);
  writeData(EPD_W >> 8);
  writeData(EPD_W & 0xFF);
  writeData(EPD_H >> 8);
  writeData(EPD_H & 0xFF);

  // Write LUTs for gray mode. CRITICAL ordering — UC8179:
  //   0x20 = LUTC  (VCOM)   ← must be present
  //   0x21 = LUTWW (W -> W)
  //   0x22 = LUTKW (K -> W)
  //   0x23 = LUTWK (W -> K)
  //   0x24 = LUTKK (K -> K)
  writeLUT(0x20, LUT_VCOM_GRAY, sizeof(LUT_VCOM_GRAY));
  checkBusy();
  writeLUT(0x21, LUT_WW_GRAY,   sizeof(LUT_WW_GRAY));
  checkBusy();
  writeLUT(0x22, LUT_KW_GRAY,   sizeof(LUT_KW_GRAY));
  checkBusy();
  writeLUT(0x23, LUT_WK_GRAY,   sizeof(LUT_WK_GRAY));
  writeLUT(0x24, LUT_KK_GRAY,   sizeof(LUT_KK_GRAY));

  Serial.println(F("[Gray4] UC8179 gray mode init done"));
}

// ============================================================
// Upload 2bpp canvas to UC8179 as two bit-planes.
// ============================================================
void uploadGray4Frame()
{
  const uint32_t bytesPerRow = EPD_W / 4; // 200 bytes (2bpp, 4 pixels/byte)

  // ---- Bit plane → DTM1 (command 0x10) ----
  writeCommand(0x10);
  hspi.beginTransaction(spiSet);
  digitalWrite(EPD_CS_PIN, LOW);
  for (uint16_t row = 0; row < EPD_H; row++) {
    const uint8_t* rp = canvas.buffer() + uint32_t(row) * bytesPerRow;
    for (uint16_t col8 = 0; col8 < EPD_W / 8; col8++) {
      uint8_t out = 0;
      for (uint8_t bit = 0; bit < 8; bit++) {
        uint16_t px = col8 * 8 + bit;
        uint32_t idx = px / 4;
        uint8_t shift = (3 - (px & 3)) * 2;
        uint8_t gray = 3 - ((rp[idx] >> shift) & 0x03);  // INVERT
        if (gray & 0x01) out |= (0x80 >> bit);
      }
      hspi.transfer(out);
    }
  }
  digitalWrite(EPD_CS_PIN, HIGH);
  hspi.endTransaction();

  // ---- Bit plane → DTM2 (command 0x13) ----
  writeCommand(0x13);
  hspi.beginTransaction(spiSet);
  digitalWrite(EPD_CS_PIN, LOW);
  for (uint16_t row = 0; row < EPD_H; row++) {
    const uint8_t* rp = canvas.buffer() + uint32_t(row) * bytesPerRow;
    for (uint16_t col8 = 0; col8 < EPD_W / 8; col8++) {
      uint8_t out = 0;
      for (uint8_t bit = 0; bit < 8; bit++) {
        uint16_t px = col8 * 8 + bit;
        uint32_t idx = px / 4;
        uint8_t shift = (3 - (px & 3)) * 2;
        uint8_t gray = 3 - ((rp[idx] >> shift) & 0x03);  // INVERT
        if (gray & 0x02) out |= (0x80 >> bit);
      }
      hspi.transfer(out);
    }
  }
  digitalWrite(EPD_CS_PIN, HIGH);
  hspi.endTransaction();

  Serial.println(F("[Gray4] Frame uploaded (2 bit planes)"));
}

void refreshDisplay()
{
  unsigned long t0 = millis();
  writeCommand(0x12); // Display Refresh
  delay(100);
  checkBusy();
  Serial.printf("[Gray4] Refresh %lu ms\n", millis() - t0);
}

void sleepDisplay()
{
  writeCommand(0x02); // Power OFF
  checkBusy();
  writeCommand(0x07); // Deep sleep
  writeData(0xA5);
}

// ============================================================
// Demo drawing
// ============================================================
void drawCenteredText(const char* text, int16_t y, const GFXfont* font, uint8_t gray)
{
  canvas.setFont(font);
  canvas.setTextColor(gray);
  int16_t tbx, tby; uint16_t tbw, tbh;
  canvas.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  canvas.setCursor((EPD_W - tbw) / 2 - tbx, y);
  canvas.print(text);
}

void showGrayscaleDemo()
{
  Serial.println(F("[Gray4] Drawing demo..."));
  canvas.fillScreen(G_WHITE);

  // Title bar
  canvas.fillRect(0, 0, EPD_W, 40, G_BLACK);
  drawCenteredText("4-Level Grayscale Demo - E1001", 30, &FreeMonoBold12pt7b, G_WHITE);

  // 4 large gray bands
  int bandH = 70, startY = 55;
  const char* labels[] = {"Gray 0: Black", "Gray 1: Dark Gray", "Gray 2: Light Gray", "Gray 3: White"};
  for (int i = 0; i < 4; i++) {
    int y = startY + i * (bandH + 8);
    canvas.fillRect(30, y, EPD_W - 60, bandH, i);
    uint8_t textGray = (i < 2) ? G_WHITE : G_BLACK;
    canvas.setFont(&FreeSansBold12pt7b);
    canvas.setTextColor(textGray);
    canvas.setCursor(60, y + bandH / 2 + 8);
    canvas.print(labels[i]);
  }

  // Concentric gray circles
  int areaTop = startY + 4 * (bandH + 8);
  int areaBot = EPD_H - 30;
  int cy = (areaTop + areaBot) / 2;
  int cx = EPD_W - 80;
  canvas.setFont(&FreeMonoBold9pt7b);
  canvas.setTextColor(G_BLACK);
  canvas.setCursor(30, cy - 6);
  canvas.print("Concentric gray circles");
  canvas.setCursor(30, cy + 14);
  canvas.print("(black / dark / light / white)");
  canvas.fillCircle(cx, cy, 38, G_BLACK);
  canvas.fillCircle(cx, cy, 28, G_DARK_GRAY);
  canvas.fillCircle(cx, cy, 18, G_LIGHT_GRAY);
  canvas.fillCircle(cx, cy,  9, G_WHITE);

  // Footer
  canvas.fillRect(0, EPD_H - 30, EPD_W, 30, G_BLACK);
  drawCenteredText("UC8179 4-gray LUT mode | E1001", EPD_H - 8, &FreeMonoBold9pt7b, G_WHITE);

  Serial.println(F("[Gray4] Uploading..."));
  uploadGray4Frame();
  refreshDisplay();
  Serial.println(F("[Gray4] Done."));
}

// ============================================================
void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println(F("[E1001 Gray4] Starting..."));

  pinMode(EPD_CS_PIN, OUTPUT);  digitalWrite(EPD_CS_PIN, HIGH);
  pinMode(EPD_DC_PIN, OUTPUT);  digitalWrite(EPD_DC_PIN, HIGH);
  pinMode(EPD_RES_PIN, OUTPUT); digitalWrite(EPD_RES_PIN, HIGH);
  pinMode(EPD_BUSY_PIN, INPUT);

  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);

  if (!canvas.begin()) {
    Serial.println(F("[Gray4] FATAL: alloc failed (96 KB)"));
    while (true) delay(1000);
  }
  Serial.printf("[Gray4] Canvas OK (%lu bytes)\n", (unsigned long)(EPD_W * EPD_H / 4));

  initGrayMode();
  showGrayscaleDemo();
  sleepDisplay();
}

void loop() {}
