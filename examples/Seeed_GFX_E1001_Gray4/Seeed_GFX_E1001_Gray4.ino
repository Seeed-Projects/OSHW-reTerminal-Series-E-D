/*
 * Seeed_GFX_E1001_Gray4 — 4-Level Grayscale Demo using Seeed_GFX / TFT_eSPI
 *
 * Compatible with : reTerminal E1001
 *
 * This example demonstrates the 4-level grayscale capability of the E1001
 * 7.5" UC8179 ePaper panel using the Seeed_GFX / TFT_eSPI high-level API.
 * It draws four horizontal bands (black, dark gray, light gray, white) and
 * then puts the display to sleep.
 *
 * How to use
 * ----------
 * 1. In driver.h, confirm BOARD_SCREEN_COMBO is 520 (already set).
 * 2. Arduino IDE board settings:
 *      Board      : XIAO_ESP32S3
 *      PSRAM      : OPI PSRAM    <-- REQUIRED
 *      Flash Size : 8 MB
 * 3. Flash to your device.
 * 4. Open a serial monitor on the carrier USB-UART bridge (GPIO43 TX / GPIO44 RX,
 *    115200 baud) — this is Serial1, not the USB-CDC Serial.
 *
 * Required libraries
 * ------------------
 *   Seeed_GFX   (provides TFT_eSPI.h and the EPaper wrapper for reTerminal E)
 *
 * Note
 * ----
 * 4-level grayscale refresh is roughly 4x slower than a 1-bit BW update.
 * The display retains the last frame without power after epaper.sleep().
 */

#include "driver.h"
#include "TFT_eSPI.h"

#ifndef EPAPER_ENABLE
#error "This example requires Setup520_Seeed_reTerminal_E1001 -- check driver.h selects BOARD_SCREEN_COMBO 520"
#endif

#define PIN_DBG_RX  44
#define PIN_DBG_TX  43
#define LOG         Serial1

EPaper epaper;

void setup()
{
  LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
  delay(500);
  LOG.println("========================================");
  LOG.println("  reTerminal E1001 Gray4 (Seeed_GFX)");
  LOG.println("========================================");

  epaper.begin();

  // Initial BW clear to reset the panel before entering gray mode.
  LOG.println("[gray4] BW clear...");
  epaper.fillScreen(TFT_WHITE);
  epaper.update();

  // Switch to 4-level gray mode and clear the gray canvas to white.
  LOG.println("[gray4] init gray mode...");
  epaper.initGrayMode(GRAY_LEVEL4);
  epaper.fillSprite(TFT_GRAY_3);

  // Draw four equal horizontal bands, one per gray level.
  const int bandH = EPD_HEIGHT / 4;
  epaper.fillRect(0, bandH * 0, EPD_WIDTH, bandH, TFT_GRAY_0);  // black
  epaper.fillRect(0, bandH * 1, EPD_WIDTH, bandH, TFT_GRAY_1);  // dark gray
  epaper.fillRect(0, bandH * 2, EPD_WIDTH, bandH, TFT_GRAY_2);  // light gray
  epaper.fillRect(0, bandH * 3, EPD_WIDTH, bandH, TFT_GRAY_3);  // white

  LOG.println("[gray4] updating (this takes a few seconds)...");
  epaper.update();

  epaper.sleep();
  LOG.println("[gray4] done.");
}

void loop() { delay(1000); }
