/*
 * Seeed_GFX_E1001_Gray4 — 4-Level Grayscale Demo using Seeed_GFX / TFT_eSPI
 *
 * Compatible with : reTerminal E1001
 *
 * Based on the official Seeed_GFX library example:
 *   File > Examples > Seeed_GFX > ePaper > Gray > GrayLevel4
 *
 * How to use
 * ----------
 * 1. In driver.h, confirm BOARD_SCREEN_COMBO is 520 (already set for E1001).
 * 2. Arduino IDE board settings:
 *      Board      : XIAO_ESP32S3
 *      PSRAM      : OPI PSRAM    <-- REQUIRED
 *      Flash Size : 8 MB
 * 3. Flash to your device.
 * 4. Open a serial monitor on the carrier USB-UART bridge (GPIO43 TX / GPIO44 RX,
 *    115200 baud) — this is Serial1, not the USB-CDC Serial.
 *
 * Display output
 * --------------
 * Step 1: Four horizontal gray bands (black / dark gray / light gray / white).
 * Step 2: A full-screen 800×480 demo image rendered in 4-level gray.
 *
 * Required libraries
 * ------------------
 *   Seeed_GFX   (provides TFT_eSPI.h and the EPaper wrapper for reTerminal E)
 */

#include "driver.h"
#include "TFT_eSPI.h"
#include "image.h"

#define PIN_DBG_RX  44
#define PIN_DBG_TX  43
#define LOG         Serial1

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

void setup()
{
  LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
  delay(500);
  LOG.println("=========================================");
  LOG.println("  reTerminal E1001 Gray4 (Seeed_GFX)");
  LOG.println("=========================================");

#ifdef EPAPER_ENABLE
  epaper.begin();

  // Initial BW clear before switching to gray mode.
  LOG.println("[gray4] BW clear...");
  epaper.fillScreen(TFT_WHITE);
  epaper.update();

  // Step 1: four horizontal gray bands.
  LOG.println("[gray4] drawing gray bands...");
  epaper.initGrayMode(GRAY_LEVEL4);
  epaper.fillRect(0, epaper.height() * 0 / 4, epaper.width(), epaper.height() / 4, TFT_GRAY_0);
  epaper.fillRect(0, epaper.height() * 1 / 4, epaper.width(), epaper.height() / 4, TFT_GRAY_1);
  epaper.fillRect(0, epaper.height() * 2 / 4, epaper.width(), epaper.height() / 4, TFT_GRAY_2);
  epaper.fillRect(0, epaper.height() * 3 / 4, epaper.width(), epaper.height() / 4, TFT_GRAY_3);
  LOG.println("[gray4] updating (takes a few seconds)...");
  epaper.update();

  // Step 2: full-screen demo image from image.h.
  LOG.println("[gray4] showing demo image...");
  epaper.fillScreen(TFT_GRAY_3);
  epaper.pushImage(0, 0, 800, 480, (uint16_t *)L4_GRAY);
  LOG.println("[gray4] updating (takes a few seconds)...");
  epaper.update();

  LOG.println("[gray4] done.");
#else
  LOG.println("[gray4] EPAPER_ENABLE not defined -- check driver.h sets BOARD_SCREEN_COMBO 520");
#endif
}

void loop()
{
}
