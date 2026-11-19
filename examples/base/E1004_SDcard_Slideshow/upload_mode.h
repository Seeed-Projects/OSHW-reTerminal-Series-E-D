/*
 * Wi-Fi upload mode for the reTerminal E10xx photo frame sketches.
 *
 * The E-series boards have no USB mass-storage path (the USB-C port is wired to
 * a CH340K UART bridge, and the ESP32-S3 native USB pins 19/20 carry the SHT4x
 * I2C bus), so photos are transferred over Wi-Fi instead:
 *
 *   board starts a WPA2 access point  ->  open http://192.168.4.1 in a browser
 *   -> drag photos -> they are written straight into IMAGE_DIR on the SD card
 *
 * The browser downscales phone photos to the largest size the board's PSRAM can
 * dither (the limits are derived from the panel geometry and dither settings
 * passed in Config), so no JPEG encoder is needed on the ESP32.
 *
 * run() never returns: it reboots the board when the user leaves upload mode, so
 * setup() runs again and rescans the card.
 */

#pragma once

#include <Arduino.h>
#include "TFT_eSPI.h"

namespace upload_mode {

struct Config {
  const char *image_dir;    // SD folder to write into, e.g. "/img"
  const char *ssid;         // soft-AP name
  const char *password;     // 8-63 chars; "" = open network
  int         exit_key_pin; // active-LOW button, hold ~1.5 s to leave
  int         buzzer_pin;   // active buzzer, -1 disables the confirmation tone
  const char *exit_hint;    // footer text on the status screen

  int      panel_w;         // panel geometry, used for the size budget
  int      panel_h;
  uint32_t sprite_bytes;    // 4bpp sprite size = panel_w * panel_h / 2
  uint8_t  bytes_per_px;    // peak bytes per source pixel for the current
                            // dither: Gray16 diffusion 6, E6 diffusion 10,
                            // BAYER8/NONE 4 (see the sketch's helper)
  uint32_t budget_bytes;    // PSRAM the photo pipeline may peak at, e.g. 7.1 MiB

  uint16_t color_bg;        // sprite nibble written as the background
  uint16_t color_fg;        // primary text
  uint16_t color_muted;     // secondary text
};

// Enters AP + HTTP upload mode until the exit key is held or the idle timeout
// fires, then restarts the ESP32.
[[noreturn]] void run(EPaper &panel, const Config &cfg);

}  // namespace upload_mode