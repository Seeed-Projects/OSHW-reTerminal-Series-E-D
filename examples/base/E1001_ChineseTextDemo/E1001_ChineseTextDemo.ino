/*
 * E1001_ChineseTextDemo
 *
 * Minimal Chinese text rendering test for reTerminal E1001.
 *
 * This sketch intentionally does only one thing: mount SPIFFS, load a TTF
 * Chinese font with OpenFontRender, draw a few UTF-8 Chinese strings, then
 * refresh the e-paper screen. It does not touch the VoiceMemoReminder example.
 *
 * Required Arduino libraries:
 *   1. Seeed_GFX
 *      - Provides TFT_eSPI.h and EPaper for the reTerminal E-series screen.
 *   2. OpenFontRender
 *      - Used to parse the TTF file and render Chinese glyphs.
 *      - If Arduino Library Manager cannot find it, install from:
 *        https://github.com/takkaO/OpenFontRender
 *
 * Required font upload step:
 *   - Do NOT use the Arduino SPIFFS Filesystem Uploader for this demo if it
 *     reports "Using partition: default_8MB". That plugin may ignore this
 *     folder's partitions.csv and build a 1.5 MB SPIFFS image, which is too
 *     small for data/font_cn.ttf.
 *   - Use the README command-line method instead. It creates a 0x4E0000-byte
 *     SPIFFS image and writes it to offset 0x310000, matching partitions.csv.
 *   - The runtime font path must be /font_cn.ttf.
 *
 * Arduino IDE board settings:
 *   - Board: XIAO ESP32S3
 *   - PSRAM: OPI PSRAM
 *   - Flash Size: 8MB
 *   - Partition Scheme: keep default; this folder's partitions.csv overrides
 *     the default 1.5MB SPIFFS with a larger SPIFFS partition for the font.
 */

#include <Arduino.h>
#include <SPIFFS.h>

#include "driver.h"
#include "OpenFontRender.h"
#include "ofrfs/M5Stack_SPIFFS_Preset.h"
#include "TFT_eSPI.h"

#define PIN_SERIAL_RX 44
#define PIN_SERIAL_TX 43

#define LOG Serial1

static const char* kFontPath = "/font_cn.ttf";

static EPaper display;
static OpenFontRender render;

static void drawBootMessage(const char* title, const char* body)
{
  display.fillSprite(TFT_GRAY_3);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_GRAY_0, TFT_GRAY_3, true);

  display.setTextSize(3);
  display.drawString(title, 48, 64);

  display.setTextSize(2);
  display.drawString(body, 48, 128);
  display.update();
}

static bool mountSpiffs()
{
  if (SPIFFS.begin(false)) {
    LOG.printf("[fs] SPIFFS mounted. total=%u used=%u\n",
               static_cast<unsigned>(SPIFFS.totalBytes()),
               static_cast<unsigned>(SPIFFS.usedBytes()));
    return true;
  }

  LOG.println("[fs] SPIFFS mount failed.");
  drawBootMessage("SPIFFS mount failed",
                  "Upload data/font_cn.ttf to a large SPIFFS partition.");
  return false;
}

static bool loadChineseFont()
{
  if (!SPIFFS.exists(kFontPath)) {
    LOG.printf("[font] missing: %s\n", kFontPath);
    drawBootMessage("Font file missing",
                    "Upload data/font_cn.ttf before flashing the sketch.");
    return false;
  }

  render.setDrawer(static_cast<TFT_eSPI&>(display));
  const int err = render.loadFont(kFontPath);
  if (err != 0) {
    LOG.printf("[font] load failed: path=%s err=%d\n", kFontPath, err);
    drawBootMessage("Font load failed",
                    "Install OpenFontRender and check /font_cn.ttf.");
    return false;
  }

  LOG.printf("[font] loaded: %s\n", kFontPath);
  return true;
}

static void drawChineseDemo()
{
  const int w = display.width();
  const int h = display.height();
  const int margin = 56;

  display.fillSprite(TFT_GRAY_3);
  display.drawRoundRect(margin - 20, 32, w - (margin - 20) * 2, h - 64,
                        12, TFT_GRAY_0);

  render.setFontColor(TFT_GRAY_0, TFT_GRAY_3);

  render.setFontSize(38);
  render.setCursor(margin, 78);
  render.printf("中文显示测试");

  render.setFontSize(30);
  render.setCursor(margin, 144);
  render.printf("你好，世界。");

  render.setFontSize(26);
  render.setCursor(margin, 204);
  render.printf("备忘录：下午三点取快递。");

  render.setFontSize(22);
  render.setCursor(margin, 266);
  render.printf("混合文本：E1001 + OpenFontRender");

  render.setFontSize(22);
  render.setCursor(margin, 324);
  render.printf("中文清晰可读，即可继续做中文版。");

  display.setTextDatum(BL_DATUM);
  display.setTextSize(2);
  display.setTextColor(TFT_GRAY_0, TFT_GRAY_3, true);
  display.drawString("Font: /font_cn.ttf", margin, h - 42);

  display.update();
}

void setup()
{
  LOG.begin(115200, SERIAL_8N1, PIN_SERIAL_RX, PIN_SERIAL_TX);
  delay(500);

  LOG.println("=========================================");
  LOG.println("  E1001 Chinese Text Demo");
  LOG.println("=========================================");

  display.begin();
  display.fillScreen(TFT_WHITE);
  display.update();
  display.initGrayMode(GRAY_LEVEL4);

  drawBootMessage("Chinese text demo", "Mounting SPIFFS and loading TTF...");

  if (!mountSpiffs()) return;
  if (!loadChineseFont()) return;

  drawChineseDemo();
  LOG.println("[demo] done.");
}

void loop()
{
}
