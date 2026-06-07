/*
 * E1003_TouchDraw — tap the touch panel and draw a dot at the same position.
 *
 * Compatible with : reTerminal E1003
 *
 * How to use
 * ----------
 * 1. Install the Seeed_GFX / TFT_eSPI display library used by reTerminal E.
 * 2. Arduino IDE board settings:
 *      Board      : XIAO_ESP32S3
 *      PSRAM      : OPI PSRAM    <-- REQUIRED. Display width/height will be 0 without it.
 *      Flash Size : 8 MB
 * 3. Flash to your device.
 * 4. Open a serial monitor on the carrier USB-UART bridge (GPIO43 TX / GPIO44 RX,
 *    115200 baud) for setup and error logs.
 * 5. Tap the screen. Each new touch draws a black dot at the touched position.
 *
 * Hardware notes
 * --------------
 * Display : E1003 10.3 inch 16-level gray ePaper.
 * Touch   : GT9xx / GT911-compatible I2C capacitive touch controller.
 * Bus     : I2C0
 *   SDA   -> GPIO19
 *   SCL   -> GPIO20
 *   INT   -> GPIO2
 *   RESET -> GPIO48
 *
 * Required libraries
 * ------------------
 *   Wire.h         (built-in I2C)
 *   Seeed_GFX      (provides TFT_eSPI.h and EPaper for reTerminal E)
 */

#include <Arduino.h>
#include <Wire.h>

#include "driver.h"
#include "TFT_eSPI.h"
#include "TouchMapper.h"

// ---------- Serial status logs (carrier USB-UART bridge) ----------
#define PIN_SERIAL_RX       44
#define PIN_SERIAL_TX       43
#define LOG                 Serial1

// ---------- E1003 touch pins from the schematic ----------
#define PIN_I2C_SDA         19
#define PIN_I2C_SCL         20
#define PIN_TOUCH_INT        2
#define PIN_TOUCH_RESET     48

// ---------- GT911 register map ----------
#define GT911_ADDR_1      0x5D
#define GT911_ADDR_2      0x14
#define GT911_REG_COMMAND 0x8040
#define GT911_REG_PRODUCT 0x8140
#define GT911_REG_STATUS  0x814E
#define GT911_REG_POINT1  0x814F
#define GT911_REG_MAX_X   0x8048

#define TOUCH_POLL_MS       30
#define DRAW_MIN_MS        450
#define DRAW_MIN_DELTA_PX   12
#define DOT_RADIUS          10

#define E1003_PANEL_WIDTH   1872
#define E1003_PANEL_HEIGHT  1404

// Set to 1 only when the panel has obvious ghosting and you want a slow
// black-white cleanup before the example screen appears. Normal boot uses one
// refresh in drawStartupScreen().
#define STRONG_BOOT_CLEAR    0

static EPaper display_;

static uint8_t s_touchAddr = 0;
static uint16_t s_touchMaxX = 1;
static uint16_t s_touchMaxY = 1;
static uint16_t s_lastRawX = 0;
static uint16_t s_lastRawY = 0;
static bool s_haveLastPoint = false;
static TouchDisplayPoint s_lastPoint = {0, 0};
static TouchDisplayPoint s_displaySize = {E1003_PANEL_WIDTH, E1003_PANEL_HEIGHT};
static bool s_displayReady = false;
static unsigned long s_lastPollMs = 0;
static unsigned long s_lastDrawMs = 0;

static void updateDisplaySize()
{
  resolveDisplaySize(static_cast<uint16_t>(display_.width()),
                     static_cast<uint16_t>(display_.height()),
                     E1003_PANEL_WIDTH,
                     E1003_PANEL_HEIGHT,
                     &s_displaySize);
}

static bool i2cRead16(uint8_t addr, uint16_t reg, uint8_t* buf, size_t len)
{
  Wire.beginTransmission(addr);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;

  const uint8_t got = Wire.requestFrom(addr, static_cast<uint8_t>(len));
  if (got != len) return false;

  for (size_t i = 0; i < len; i++) {
    buf[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

static bool i2cWrite16(uint8_t addr, uint16_t reg, uint8_t value)
{
  Wire.beginTransmission(addr);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static void resetTouchController()
{
  pinMode(PIN_TOUCH_INT, INPUT);
  pinMode(PIN_TOUCH_RESET, OUTPUT);

  digitalWrite(PIN_TOUCH_RESET, LOW);
  delay(20);
  digitalWrite(PIN_TOUCH_RESET, HIGH);
  delay(120);
}

static bool probeGt911(uint8_t addr)
{
  uint8_t product[4] = {};
  if (!i2cRead16(addr, GT911_REG_PRODUCT, product, sizeof(product))) {
    return false;
  }
  LOG.printf("[touch] GT9xx found at 0x%02X, product: %c%c%c%c\n",
             addr, product[0], product[1], product[2], product[3]);
  return true;
}

static void readTouchLimits()
{
  uint8_t raw[4] = {};
  if (!i2cRead16(s_touchAddr, GT911_REG_MAX_X, raw, sizeof(raw))) {
    s_touchMaxX = s_displaySize.x;
    s_touchMaxY = s_displaySize.y;
    return;
  }

  const uint16_t maxX = static_cast<uint16_t>(raw[0] | (raw[1] << 8));
  const uint16_t maxY = static_cast<uint16_t>(raw[2] | (raw[3] << 8));

  if (maxX > 0 && maxY > 0) {
    s_touchMaxX = maxX;
    s_touchMaxY = maxY;
  }

  LOG.printf("[touch] Touch range: %u x %u, display: %u x %u\n",
             s_touchMaxX, s_touchMaxY, s_displaySize.x, s_displaySize.y);
}

static bool initTouch()
{
  resetTouchController();

  if (probeGt911(GT911_ADDR_1)) {
    s_touchAddr = GT911_ADDR_1;
  } else if (probeGt911(GT911_ADDR_2)) {
    s_touchAddr = GT911_ADDR_2;
  } else {
    LOG.println("[touch] GT9xx touch controller not found.");
    return false;
  }

  readTouchLimits();
  i2cWrite16(s_touchAddr, GT911_REG_COMMAND, 0x00);
  i2cWrite16(s_touchAddr, GT911_REG_STATUS, 0x00);
  pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
  LOG.println("[touch] Ready.");
  return true;
}

static bool readTouchPoint(TouchDisplayPoint* point)
{
  uint8_t status = 0;
  if (!i2cRead16(s_touchAddr, GT911_REG_STATUS, &status, 1)) {
    LOG.println("[touch] Failed to read GT911 status register.");
    return false;
  }

  const int intLevel = digitalRead(PIN_TOUCH_INT);
  const uint8_t pointCount = status & 0x0F;
  if (!gt911StatusRequestsRead(status, intLevel)) {
    return false;
  }

  uint8_t raw[8] = {};
  const bool ok = i2cRead16(s_touchAddr, GT911_REG_POINT1, raw, sizeof(raw));
  i2cWrite16(s_touchAddr, GT911_REG_STATUS, 0x00);
  if (!ok) {
    LOG.println("[touch] Failed to read GT911 point data.");
    return false;
  }

  if (pointCount == 0 && (raw[1] == 0 && raw[2] == 0 && raw[3] == 0 && raw[4] == 0)) {
    return false;
  }

  const uint16_t rawX = static_cast<uint16_t>(raw[1] | (raw[2] << 8));
  const uint16_t rawY = static_cast<uint16_t>(raw[3] | (raw[4] << 8));
  s_lastRawX = rawX;
  s_lastRawY = rawY;
  const bool mapped = mapTouchToDisplay(rawX, rawY, s_touchMaxX, s_touchMaxY,
                                        s_displaySize.x,
                                        s_displaySize.y,
                                        point);
  return mapped;
}

static bool shouldDrawPoint(const TouchDisplayPoint& point)
{
  const unsigned long now = millis();
  if (!s_haveLastPoint) return true;
  if (now - s_lastDrawMs < DRAW_MIN_MS) return false;

  const int dx = abs(static_cast<int>(point.x) - static_cast<int>(s_lastPoint.x));
  const int dy = abs(static_cast<int>(point.y) - static_cast<int>(s_lastPoint.y));
  return dx >= DRAW_MIN_DELTA_PX || dy >= DRAW_MIN_DELTA_PX;
}

static void drawStartupScreen(bool touchReady)
{
  if (!s_displayReady) return;

  display_.fillSprite(TFT_WHITE);
  display_.setTextDatum(TC_DATUM);
  display_.setTextColor(TFT_BLACK, TFT_WHITE, true);
  display_.setTextSize(5);
  display_.drawString("E1003 Touch Draw", display_.width() / 2, 90);

  display_.setTextSize(3);
  display_.drawString(touchReady ? "Tap anywhere to draw dots." : "Touch controller not found.",
                      display_.width() / 2, 180);
  display_.drawFastHLine(80, 260, display_.width() - 160, TFT_BLACK);
  display_.update();
}

static void drawPoint(const TouchDisplayPoint& point)
{
  if (!s_displayReady) return;

  display_.fillCircle(point.x, point.y, DOT_RADIUS, TFT_BLACK);
  display_.drawCircle(point.x, point.y, DOT_RADIUS + 4, TFT_GRAY_6);
  display_.update();
  LOG.printf("[touch] raw=(%u,%u) screen=(%u,%u)\n",
             s_lastRawX, s_lastRawY, point.x, point.y);

  s_lastPoint = point;
  s_haveLastPoint = true;
  s_lastDrawMs = millis();
}

static bool setupDisplay()
{
  LOG.printf("[display] PSRAM found: %s, free PSRAM: %u bytes\n",
             psramFound() ? "yes" : "no",
             static_cast<unsigned>(ESP.getFreePsram()));

  if (!psramFound()) {
    LOG.println("[display] ERROR: enable Tools -> PSRAM -> OPI PSRAM.");
    return false;
  }

  display_.begin();
  updateDisplaySize();

  if (display_.width() == 0 || display_.height() == 0) {
    LOG.println("[display] ERROR: 1-bit ePaper buffer was not created.");
    return false;
  }

#if STRONG_BOOT_CLEAR
  LOG.println("[display] Clearing old ePaper image...");
  display_.fillScreen(TFT_BLACK);
  display_.update();
  delay(800);

  display_.fillScreen(TFT_WHITE);
  display_.update();
  delay(800);
#endif

  display_.initGrayMode(GRAY_LEVEL16);
  updateDisplaySize();

  if (display_.width() == 0 || display_.height() == 0) {
    LOG.println("[display] ERROR: 16-gray ePaper buffer was not created.");
    return false;
  }

  LOG.printf("[display] Ready: %u x %u\n", s_displaySize.x, s_displaySize.y);
  return true;
}

void setup()
{
  LOG.begin(115200, SERIAL_8N1, PIN_SERIAL_RX, PIN_SERIAL_TX);
  delay(100);

  LOG.println("========================================");
  LOG.println("  E1003_TouchDraw - reTerminal E1003");
  LOG.println("========================================");

  s_displayReady = setupDisplay();
  s_touchMaxX = s_displaySize.x;
  s_touchMaxY = s_displaySize.y;

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000UL);

  const bool touchReady = initTouch();
  drawStartupScreen(touchReady);

  if (!s_displayReady) {
    LOG.println("[hint] Display is not ready; check PSRAM and Seeed_GFX setup.");
  }
}

void loop()
{
  if (s_touchAddr == 0) {
    delay(1000);
    return;
  }

  const unsigned long now = millis();
  if (now - s_lastPollMs < TOUCH_POLL_MS) return;
  s_lastPollMs = now;

  TouchDisplayPoint point = {};
  if (readTouchPoint(&point) && shouldDrawPoint(point)) {
    drawPoint(point);
  }
}
