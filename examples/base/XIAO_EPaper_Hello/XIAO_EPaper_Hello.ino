/*
 * XIAO ePaper DIY Kit - Hello ePaper
 *
 * Smoke-test firmware for XIAO ePaper driver boards (EE02-EE05 / EN04-EN05).
 * The board + panel combination is selected at compile time in driver.h,
 * so one binary matches exactly one hardware pairing.
 *
 * XIAO ePaper DIY Kit - Hello ePaper 点屏固件。
 * 「板 + 屏」组合在 driver.h 里于编译期选定，一个固件对应一种硬件搭配。
 */

#include "driver.h"
#include "TFT_eSPI.h"

#ifndef EPAPER_ENABLE
#error "BOARD_SCREEN_COMBO in driver.h must select an ePaper setup (502-517)"
#endif

EPaper epaper;

// Draws a frame, title, and hardware summary scaled to the panel size.
// 按屏幕尺寸绘制边框、标题和硬件信息。
static void drawHelloScreen() {
  const int32_t w = epaper.width();
  const int32_t h = epaper.height();
  // Text scale grows with panel width: 1 on 200px, up to 6 on 1600px.
  // 字号随屏宽增长：200px 宽为 1，1600px 宽最大到 6。
  const uint8_t scale = w >= 1200 ? 6 : w >= 760 ? 3 : w >= 380 ? 2 : 1;
  const int32_t margin = 4 * scale;

#if defined(USE_COLORFULL_EPAPER) || defined(USE_BWRY_EPAPER)
  const uint32_t accent = TFT_RED;
#else
  const uint32_t accent = TFT_BLACK;
#endif

  epaper.fillScreen(TFT_WHITE);
  epaper.drawRect(margin, margin, w - 2 * margin, h - 2 * margin, TFT_BLACK);

  epaper.setTextColor(accent, TFT_WHITE, true);
  epaper.setTextSize(scale);
  epaper.drawString("Hello ePaper", margin * 3, margin * 4);

  epaper.setTextColor(TFT_BLACK, TFT_WHITE, true);
  epaper.drawString("Board: " HELLO_BOARD_NAME, margin * 3, margin * 4 + 12 * scale);

  char line[32];
  snprintf(line, sizeof(line), "Panel: %ldx%ld", (long)w, (long)h);
  epaper.drawString(line, margin * 3, margin * 4 + 24 * scale);

  epaper.fillCircle(w - margin * 6, h - margin * 6, 2 * margin, accent);
}

void setup() {
  Serial.begin(115200);

  epaper.begin();
  drawHelloScreen();
  epaper.update();
  epaper.sleep();

  Serial.println("Hello ePaper: display updated");
}

void loop() {
}
