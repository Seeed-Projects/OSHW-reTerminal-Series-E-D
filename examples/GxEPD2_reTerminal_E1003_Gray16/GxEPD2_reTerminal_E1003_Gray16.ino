#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// ===== Pin mapping (E1003) =====
#define EPD_SCK_PIN     7
#define EPD_MISO_PIN    8
#define EPD_MOSI_PIN    9
#define EPD_CS_PIN      10
#define EPD_BUSY_PIN    13  // HRDY
#define EPD_TFT_ENABLE  11
#define EPD_ITE_ENABLE  21

// Panel geometry
#define EPD_W  1872
#define EPD_H  1404

SPIClass hspi(HSPI);
SPISettings spiSet(10000000, MSBFIRST, SPI_MODE0);

// IT8951 commands
#define CMD_SYS_RUN       0x0001
#define CMD_STANDBY       0x0002
#define CMD_SLEEP         0x0003
#define CMD_REG_RD        0x0010
#define CMD_REG_WR        0x0011
#define CMD_LD_IMG_AREA   0x0021
#define CMD_LD_IMG_END    0x0022
#define CMD_DPY_AREA      0x0034
#define REG_LISAR         0x0208

#define IT8951_8BPP        3
#define IT8951_B_ENDIAN    1
#define IT8951_ROTATE_0    0

#define DBG Serial0

// ============================================================
// Minimal 16-level grayscale canvas (4bpp, PSRAM-backed)
// ============================================================
class Gray16Canvas : public Adafruit_GFX
{
public:
  Gray16Canvas(uint16_t w, uint16_t h) : Adafruit_GFX(w, h), _buf(nullptr) {}

  bool begin()
  {
    uint32_t sz = uint32_t(_width) * _height / 2;
#if defined(ESP32)
    _buf = (uint8_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    _buf = (uint8_t*)malloc(sz);
#endif
    return _buf != nullptr;
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override
  {
    if (!_buf) return;
    if (x < 0 || x >= _width || y < 0 || y >= _height) return;
    switch (getRotation()) {
      case 1: { int16_t t = x; x = _width - 1 - y; y = t; break; }
      case 2: x = _width - 1 - x; y = _height - 1 - y; break;
      case 3: { int16_t t = x; x = y; y = _height - 1 - t; break; }
    }
    uint8_t g = color & 0x0F;
    uint32_t idx = uint32_t(y) * (_width / 2) + x / 2;
    if (x & 1) _buf[idx] = (_buf[idx] & 0xF0) | g;
    else       _buf[idx] = (_buf[idx] & 0x0F) | (g << 4);
  }

  void fillScreen(uint16_t color) override
  {
    if (!_buf) return;
    uint8_t g = color & 0x0F;
    memset(_buf, (g << 4) | g, uint32_t(_width) * _height / 2);
  }

  uint8_t getPixel4bpp(int16_t x, int16_t y) const
  {
    if (!_buf || x < 0 || x >= _width || y < 0 || y >= _height) return 0;
    uint32_t idx = uint32_t(y) * (_width / 2) + x / 2;
    return (x & 1) ? (_buf[idx] & 0x0F) : ((_buf[idx] >> 4) & 0x0F);
  }

  uint8_t* buffer() { return _buf; }

private:
  uint8_t* _buf;
};

Gray16Canvas canvas(EPD_W, EPD_H);

// IT8951 device info
uint32_t imgBufAddr = 0;

// ============================================================
// IT8951 SPI helpers
// ============================================================
void waitHRDY() { while (digitalRead(EPD_BUSY_PIN) == LOW) delay(1); }

uint16_t xfer16(uint16_t v) {
  uint16_t r = hspi.transfer16(v);
  return r;
}

void writeCmd16(uint16_t cmd) {
  waitHRDY();
  hspi.beginTransaction(spiSet);
  digitalWrite(EPD_CS_PIN, LOW);
  xfer16(0x6000);
  waitHRDY();
  xfer16(cmd);
  digitalWrite(EPD_CS_PIN, HIGH);
  hspi.endTransaction();
}

void writeData16(uint16_t data) {
  waitHRDY();
  hspi.beginTransaction(spiSet);
  digitalWrite(EPD_CS_PIN, LOW);
  xfer16(0x0000);
  waitHRDY();
  xfer16(data);
  digitalWrite(EPD_CS_PIN, HIGH);
  hspi.endTransaction();
}

uint16_t readData16() {
  waitHRDY();
  hspi.beginTransaction(spiSet);
  digitalWrite(EPD_CS_PIN, LOW);
  xfer16(0x1000);
  waitHRDY();
  xfer16(0);          // dummy
  uint16_t v = xfer16(0);
  digitalWrite(EPD_CS_PIN, HIGH);
  hspi.endTransaction();
  return v;
}

void writeReg(uint16_t reg, uint16_t val) {
  writeCmd16(CMD_REG_WR);
  writeData16(reg);
  writeData16(val);
}

uint16_t readReg(uint16_t reg) {
  writeCmd16(CMD_REG_RD);
  writeData16(reg);
  return readData16();
}

void readDevInfo() {
  writeCmd16(0x0302);
  uint16_t buf[20];
  waitHRDY();
  hspi.beginTransaction(spiSet);
  digitalWrite(EPD_CS_PIN, LOW);
  xfer16(0x1000);
  waitHRDY();
  xfer16(0);
  for (int i = 0; i < 20; i++) buf[i] = xfer16(0);
  digitalWrite(EPD_CS_PIN, HIGH);
  hspi.endTransaction();
  imgBufAddr = (uint32_t(buf[3]) << 16) | buf[2];
  DBG.printf("[IT8951] panel %ux%u, imgBuf=0x%08lX\n", buf[0], buf[1], imgBufAddr);
}

void setVCOM(uint16_t mV) {
  writeCmd16(0x0039);
  writeData16(0x0001);
  writeData16(mV);
}

void setTemperature(uint16_t t) {
  writeCmd16(0x0040);
  writeData16(0x0001);
  writeData16(t);
}

// ============================================================
// Upload 4bpp framebuffer to IT8951 as 8BPP, X-mirrored
// ============================================================
void uploadGray16Frame()
{
  writeReg(REG_LISAR,     (uint16_t)(imgBufAddr & 0xFFFF));
  writeReg(REG_LISAR + 2, (uint16_t)(imgBufAddr >> 16));

  setTemperature(16);

  uint16_t args[5];
  args[0] = (IT8951_B_ENDIAN << 8) | (IT8951_8BPP << 4) | IT8951_ROTATE_0;
  args[1] = 0;
  args[2] = 0;
  args[3] = EPD_W;
  args[4] = EPD_H;
  writeCmd16(CMD_LD_IMG_AREA);
  for (int i = 0; i < 5; i++) writeData16(args[i]);

  unsigned long t0 = millis();
  hspi.beginTransaction(spiSet);
  digitalWrite(EPD_CS_PIN, LOW);
  waitHRDY();
  xfer16(0x0000);
  waitHRDY();

  const uint16_t WB = EPD_W / 2;
  for (uint16_t row = 0; row < EPD_H; row++) {
    const uint8_t* rp = canvas.buffer() + uint32_t(row) * WB;
    // X-mirror: send bytes right-to-left, nibbles LSB-first within each byte
    for (int16_t col = WB - 1; col >= 0; col--) {
      uint8_t byte_val = rp[col];
      uint8_t lo = byte_val & 0x0F;
      uint8_t hi = (byte_val >> 4) & 0x0F;
      hspi.transfer(lo * 17);  // low nibble = right pixel (sent first due to X-mirror)
      hspi.transfer(hi * 17);  // high nibble = left pixel
    }
    if ((row & 0x3F) == 0) yield();
  }

  digitalWrite(EPD_CS_PIN, HIGH);
  hspi.endTransaction();

  writeCmd16(CMD_LD_IMG_END);
  waitHRDY();

  DBG.printf("[Gray16] upload %lu ms\n", millis() - t0);
}

void displayRefreshGC16()
{
  unsigned long t0 = millis();
  writeCmd16(CMD_DPY_AREA);
  waitHRDY(); writeData16(0);
  waitHRDY(); writeData16(0);
  waitHRDY(); writeData16(EPD_W);
  waitHRDY(); writeData16(EPD_H);
  waitHRDY(); writeData16(2);  // mode 2 = GC16

  while (digitalRead(EPD_BUSY_PIN) == LOW) {
    if (millis() - t0 > 15000) break;
    delay(100);
  }
  DBG.printf("[Gray16] refresh %lu ms\n", millis() - t0);
}

// ============================================================
// Demo drawing functions
// ============================================================

// Gray colors 0-15 (0=black, 15=white)
#define G_BLACK   0
#define G_WHITE  15

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
  DBG.println(F("[Gray16] Drawing grayscale demo..."));
  canvas.fillScreen(G_WHITE);

  // Title
  canvas.fillRect(0, 0, EPD_W, 60, G_BLACK);
  drawCenteredText("16-Level Grayscale Demo - E1003", 45, &FreeMonoBold12pt7b, G_WHITE);

  // 16 horizontal gray bands
  int bandH = 60, startY = 80;
  canvas.setFont(&FreeMonoBold9pt7b);
  for (int i = 0; i < 16; i++) {
    int y = startY + i * bandH;
    canvas.fillRect(50, y, EPD_W - 100, bandH - 4, i);
    uint8_t textColor = (i < 8) ? 15 : 0;
    canvas.setTextColor(textColor);
    char buf[32];
    snprintf(buf, sizeof(buf), "Gray %2d  (8bpp = %3d)", i, i * 17);
    canvas.setCursor(80, y + bandH / 2 + 5);
    canvas.print(buf);
  }

  // Gradient bar (smooth)
  int gradY = startY + 16 * bandH + 20;
  canvas.setFont(&FreeMonoBold9pt7b);
  canvas.setTextColor(G_BLACK);
  canvas.setCursor(50, gradY - 5);
  canvas.print("Smooth 16-step gradient:");
  for (int x = 0; x < EPD_W - 100; x++) {
    uint8_t g = (x * 15) / (EPD_W - 101);
    canvas.drawFastVLine(50 + x, gradY + 10, 60, g);
  }
  canvas.drawRect(49, gradY + 9, EPD_W - 98, 62, G_BLACK);

  // Concentric circles with gray levels
  int cx = EPD_W / 2;
  int cy = gradY + 102;
  canvas.setTextColor(G_BLACK);
  canvas.setCursor(50, cy);
  canvas.print("Concentric gray circles:");
  cy += 35;
  for (int r = 7; r >= 0; r--) {
    int radius = 24 + r * 12;
    canvas.fillCircle(cx, cy + 45, radius, r * 2);
  }

  DBG.println(F("[Gray16] Uploading..."));
  uploadGray16Frame();
  displayRefreshGC16();
  DBG.println(F("[Gray16] Done."));
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println(F("[E1003 Gray16] Starting..."));

  pinMode(EPD_CS_PIN, OUTPUT);
  digitalWrite(EPD_CS_PIN, HIGH);
  pinMode(EPD_BUSY_PIN, INPUT);
  pinMode(EPD_TFT_ENABLE, OUTPUT);
  digitalWrite(EPD_TFT_ENABLE, HIGH);
  pinMode(EPD_ITE_ENABLE, OUTPUT);
  digitalWrite(EPD_ITE_ENABLE, HIGH);

  hspi.begin(EPD_SCK_PIN, EPD_MISO_PIN, EPD_MOSI_PIN, -1);

  delay(100);
  writeCmd16(CMD_SYS_RUN);
  delay(100);

  readDevInfo();
  setVCOM(1400);

  if (!canvas.begin()) {
    DBG.println(F("[Gray16] FATAL: PSRAM alloc failed (~1.25 MB needed)"));
    while (true) delay(1000);
  }
  DBG.printf("[Gray16] Framebuffer OK (%lu bytes)\n", (unsigned long)(EPD_W * EPD_H / 2));

  showGrayscaleDemo();
}

void loop() {}
