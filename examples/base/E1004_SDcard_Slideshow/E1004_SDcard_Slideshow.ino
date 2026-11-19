/*
 * reTerminal E1004 -- SD card photo slideshow (6-color, 1200x1600).
 *
 * Based on Seeed_GFX/examples/ePaper/reTerminal_SDcard_Bitmap/reTerminal_E1004_SDcard_Color6.
 * Same route as the E1003 sketch: keep the decode -> dither -> push pipeline, add
 * a directory scan, a timed loop, page-turn keys and a Wi-Fi upload mode.
 *
 * Pipeline (one image per slide):
 *   SD JPG/BMP/PNG -> RGB888 -> 6-color quantize + dither -> 4bpp packed
 *               -> pushImage into the 4bpp T133A01 sprite -> epaper.update()
 *
 * 6-color palette (E6 codes used by dither.cpp):
 *   0x0 WHITE  0x2 GREEN  0x6 RED  0xB YELLOW  0xD BLUE  0xF BLACK
 * Note the sprite stores these raw codes, so WHITE is nibble 0, not 0xF.
 *
 * MEMORY BUDGET (1200x1600 panel + XIAO ESP32-S3 8 MB PSRAM) -- READ THIS:
 *   - 4bpp sprite          : 1200*1600 / 2 = 0.92 MB (allocated by EPaper)
 *   - Source RGB888        : width*height*3 bytes
 *   - Quantization index   : width*height bytes
 *   - FS/Jarvis error buf  : only maxdy+1 rows (~13-20 KB), ring buffer
 *   Peak ~= 0.92 MB + 4 * width * height  (all methods)
 *
 *   At full panel size (1200x1600 = 1.92 Mpx) even BAYER8 needs ~8.6 MB, so
 *   photos must be pre-scaled. Practical upload sizes:
 *     every method (BN / BAYER8 / FS / Jarvis) -- about 1110 x 1482
 *   The Wi-Fi upload page asks the board for these limits and scales phone
 *   photos automatically (see upload_mode.h).
 *
 * SD card requirements:
 *   - FAT32 (exFAT also works on recent cores), <= 64 GB
 *   - Put photos in /img (or change IMAGE_DIR below). Files are sorted by name.
 *   - Supported: baseline JPEG, BMP (24-bit or 4-bit indexed), PNG.
 *
 * Hardware pins (reTerminal E1004):
 *   SPI SCK 7 / MISO 8 / MOSI 9 (shared with the panel)
 *   EPD CS 10, CS1 2, DC 11, RST 38, BUSY 13, ENABLE 12
 *   SD  CS 14, SD_EN 16 (E1003 uses 39), SD_DET 15
 *   Keys KEY0/1/2 = GPIO3/4/5 (right/middle/left), buzzer GPIO45
 *
 * Logs go out on UART1 (GPIO43 TX / GPIO44 RX) at 115200 -- that is the
 * reTerminal carrier's USB-to-UART bridge.
 *
 * Arduino IDE / arduino-cli settings:
 *   Board        : XIAO_ESP32S3
 *   PSRAM        : OPI PSRAM      <-- REQUIRED, otherwise everything OOMs
 *   Flash Size   : 8MB
 *   Partition    : Default 8MB (3MB APP)
 */

#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include "TFT_eSPI.h"

#include <vector>
#include <algorithm>

#include "dither.h"
#include "image_loader.h"
#include "upload_mode.h"

#ifndef EPAPER_ENABLE
#error "This sketch requires Setup523_Seeed_reTerminal_E1004 -- check driver.h selects BOARD_SCREEN_COMBO 523"
#endif

EPaper epaper;

// ----- pins ------------------------------------------------------------------
// SD pins are identical across all reTerminal E10xx carrier boards, except
// SD_EN: E1001/E1002/E1004 use GPIO16, E1003 uses GPIO39.
static constexpr int    PIN_SD_SCK   = 7;    // shared with e-paper SPI bus
static constexpr int    PIN_SD_MISO  = 8;    // already in Setup523 (TFT_MISO=8)
static constexpr int    PIN_SD_MOSI  = 9;    // shared with e-paper SPI bus
static constexpr int    PIN_SD_CS    = 14;
static constexpr int    PIN_SD_EN    = 16;   // E1004
static constexpr int    PIN_SD_DET   = 15;
static constexpr int    PIN_DBG_RX   = 44;
static constexpr int    PIN_DBG_TX   = 43;
// User buttons (E10xx): right / middle / left, active LOW.
static constexpr int    PIN_KEY0     = 3;    // right
static constexpr int    PIN_KEY1     = 4;    // middle
static constexpr int    PIN_KEY2     = 5;    // left
static constexpr int    PIN_BUZZER   = 45;   // onboard buzzer
static constexpr int    PANEL_W      = 1200;
static constexpr int    PANEL_H      = 1600;
#define LOG       Serial1
#define TAG       "[e1004-slideshow]"

// E6 color codes as stored in the 4bpp sprite (see dither.h PAL_E6).
static constexpr uint16_t E6_WHITE  = 0x0;
static constexpr uint16_t E6_GREEN  = 0x2;
static constexpr uint16_t E6_RED    = 0x6;
static constexpr uint16_t E6_YELLOW = 0xB;
static constexpr uint16_t E6_BLUE   = 0xD;
static constexpr uint16_t E6_BLACK  = 0xF;

// =============================================================================
// USER CONFIGURATION -- edit the constants below.
// =============================================================================

// ----- slideshow -------------------------------------------------------------
// Folder on the SD card to scan. All supported images in this folder are shown,
// sorted alphabetically. If the folder does not exist, the card root is used.
static const char* IMAGE_DIR = "/img";

// Time each slide stays on screen, in milliseconds.
// Keep this comfortably above the per-slide processing time (~5-10 s on E1004).
static const uint32_t SLIDE_INTERVAL_MS = 30000;

// Maximum number of files remembered. 128 is plenty for a photo frame.
static const size_t MAX_FILES = 128;

// ----- page-turn buttons -----------------------------------------------------
// E10xx physical layout (front view): KEY0 = right, KEY1 = middle, KEY2 = left.
// All keys are active LOW with external pull-ups.
// Default mapping: right = next page, middle = previous page; hold left = upload.
static constexpr int PIN_KEY_NEXT   = PIN_KEY0;   // right button  -> next slide
static constexpr int PIN_KEY_PREV   = PIN_KEY1;   // middle button -> previous slide
static constexpr int PIN_KEY_UPLOAD = PIN_KEY2;   // left button   -> hold for Wi-Fi upload

// Ignore additional presses for this long after an accepted press. Also acts
// as the debounce window for these mechanical buttons.
static constexpr uint32_t KEY_LOCKOUT_MS = 400;

// ----- Wi-Fi upload mode (AP) ------------------------------------------------
// Hold the LEFT key for UPLOAD_HOLD_MS and the board starts a WPA2 access
// point with a small web page. Open http://192.168.4.1 on your computer or
// phone, drag photos in -- they are written straight to IMAGE_DIR, no card
// reader needed. Hold the same key again to leave; the board reboots.
// USB mass storage is not possible on this board: the USB-C connector goes to
// a CH340K UART bridge, and the ESP32-S3 native USB pins (GPIO19/20) carry the
// SHT4x I2C bus instead.
static const char*    AP_SSID      = "E1004-Photo";
static const char*    AP_PASSWORD  = "e1004photo";  // 8-63 chars; "" = open network
static constexpr uint32_t UPLOAD_HOLD_MS = 2000;    // hold left key this long

// PSRAM ceiling used for the upload page's size budget.
static constexpr uint32_t UPLOAD_BUDGET_BYTES = (uint32_t)(7.2 * 1024.0 * 1024.0);

// ----- key-press beep --------------------------------------------------------
// The e-paper takes several seconds to redraw, so give immediate audible
// feedback that the press was accepted.
static constexpr bool         BUZZER_FEEDBACK = true;
static constexpr unsigned int BEEP_NEXT_HZ    = 2000;  // left key   (next)
static constexpr unsigned int BEEP_PREV_HZ    = 1200;  // middle key (previous)
static constexpr unsigned long BEEP_MS        = 45;

// ----- dither ----------------------------------------------------------------
//   DITHER_NONE     -- nearest-color, no dithering (fastest, blocky)
//   DITHER_BAYER8   -- 8x8 ordered Bayer (no error buffer; allows larger sources)
//   DITHER_BN       -- 32x32 blue-noise ordered (same size budget, aperiodic:
//                      gradients read as grain instead of a cross-hatch)
//   DITHER_YLO      -- per-pixel best pair of inks + blue-noise threshold
//                      (best colour accuracy on a 6-ink panel)
//   DITHER_FS       -- Floyd-Steinberg (best quality, needs a 2-byte/px buffer)
//   DITHER_JARVIS   -- Jarvis-Judice-Ninke (smoother, slowest)
//   DITHER_ATKINSON -- Atkinson (high contrast, classic Mac look)
// DITHER_BN = 32x32 blue-noise ordered dither: same dot density as Bayer but
// aperiodic, so gradients read as fine grain instead of a cross-hatch.
// (BAYER8 remains the fallback; the E6 error-diffusion buffer is three int16
// per pixel, so FS/Jarvis only fit at ~700x936 -- too small to use.)
static const DitherMethod DITHER_METHOD = DITHER_YLO;

// Brightness gamma. 1.0 = neutral, >1.0 darkens, <1.0 brightens. Typical 0.8-1.6.
static const float DITHER_GAMMA = 1.0f;

// ----- layout ----------------------------------------------------------------
enum DisplayAnchor {
  ANCHOR_TOP_LEFT,      ANCHOR_TOP_CENTER,      ANCHOR_TOP_RIGHT,
  ANCHOR_MIDDLE_LEFT,   ANCHOR_CENTER,          ANCHOR_MIDDLE_RIGHT,
  ANCHOR_BOTTOM_LEFT,   ANCHOR_BOTTOM_CENTER,   ANCHOR_BOTTOM_RIGHT,
};
static const DisplayAnchor DISPLAY_ANCHOR = ANCHOR_CENTER;

enum DisplayFit { FIT_ORIGINAL, FIT_CONTAIN, FIT_SCALE };
// FIT_CONTAIN shrinks oversized photos to fit the panel and never upscales.
static const DisplayFit DISPLAY_FIT = FIT_CONTAIN;

// Only used when DISPLAY_FIT == FIT_SCALE (0.25 / 0.5 / 1.0 / 2.0 ...).
static const float DISPLAY_SCALE = 1.0f;

// =============================================================================
// END USER CONFIGURATION
// =============================================================================

// ----- slideshow state -------------------------------------------------------
static std::vector<String> g_files;
static size_t              g_next      = 0;   // index of the next auto slide
static size_t              g_current   = 0;   // index of the slide on screen
static bool                g_haveCurrent = false;
static uint32_t            g_lastShow  = 0;   // millis() of the last successful slide

// Page-turn key state (edge detection + lockout).
static bool                g_nextKeyWasDown = false;
static bool                g_prevKeyWasDown = false;
static uint32_t            g_lastKeyMs      = 0;

// Long-press detection for the left key (opens Wi-Fi upload mode), and the
// "held while booting" flag used by setup().
static uint32_t            g_uploadDownMs    = 0;
static bool                g_uploadLongFired = false;
static bool                g_uploadWasDown   = false;
static bool                g_bootHoldWanted  = false;

// ----- diagnostics helpers ----------------------------------------------------
static void log_mem(const char* tag) {
  LOG.printf("[mem] %-22s heap=%lu kB  PSRAM free=%lu/%lu kB\n", tag,
             (unsigned long)(ESP.getFreeHeap() / 1024),
             (unsigned long)(ESP.getFreePsram() / 1024),
             (unsigned long)(ESP.getPsramSize() / 1024));
  LOG.flush();
}

static void list_sd_root(int max_entries = 32) {
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    LOG.println("[sd] cannot open '/' on the card");
    if (root) root.close();
    return;
  }
  LOG.println("[sd] contents of '/' :");
  int n = 0;
  File entry = root.openNextFile();
  while (entry) {
    if (entry.isDirectory()) LOG.printf("   <DIR>  %s\n", entry.name());
    else                     LOG.printf("   %7lu B  %s\n",
                                        (unsigned long)entry.size(), entry.name());
    entry.close();
    if (++n >= max_entries) { LOG.printf("   ... (truncated at %d entries)\n", max_entries); break; }
    entry = root.openNextFile();
  }
  if (n == 0) LOG.println("   (empty)");
  root.close();
  LOG.flush();
}

// ----- layout helpers ---------------------------------------------------------
static void compute_target_size(int src_w, int src_h, DisplayFit fit, float scale,
                                int panel_w, int panel_h,
                                int* out_w, int* out_h) {
  switch (fit) {
    case FIT_ORIGINAL:
      *out_w = src_w; *out_h = src_h;
      break;
    case FIT_CONTAIN: {
      double sx = (double)panel_w / src_w;
      double sy = (double)panel_h / src_h;
      double s  = (sx < sy) ? sx : sy;
      if (s > 1.0) s = 1.0;
      *out_w = (int)(src_w * s + 0.5);
      *out_h = (int)(src_h * s + 0.5);
      break;
    }
    case FIT_SCALE:
      *out_w = (int)(src_w * scale + 0.5);
      *out_h = (int)(src_h * scale + 0.5);
      break;
  }
  if (*out_w & 1) (*out_w)--;
  if (*out_h & 1) (*out_h)--;
  if (*out_w < 2) *out_w = 2;
  if (*out_h < 2) *out_h = 2;
}

static void compute_anchor_xy(int img_w, int img_h, DisplayAnchor a,
                              int panel_w, int panel_h,
                              int* out_x, int* out_y) {
  int x = 0, y = 0;
  switch (a) {
    case ANCHOR_TOP_LEFT:      x = 0;                      y = 0;                       break;
    case ANCHOR_TOP_CENTER:    x = (panel_w - img_w) / 2;  y = 0;                       break;
    case ANCHOR_TOP_RIGHT:     x = panel_w - img_w;        y = 0;                       break;
    case ANCHOR_MIDDLE_LEFT:   x = 0;                      y = (panel_h - img_h) / 2;   break;
    case ANCHOR_CENTER:        x = (panel_w - img_w) / 2;  y = (panel_h - img_h) / 2;   break;
    case ANCHOR_MIDDLE_RIGHT:  x = panel_w - img_w;        y = (panel_h - img_h) / 2;   break;
    case ANCHOR_BOTTOM_LEFT:   x = 0;                      y = panel_h - img_h;         break;
    case ANCHOR_BOTTOM_CENTER: x = (panel_w - img_w) / 2;  y = panel_h - img_h;         break;
    case ANCHOR_BOTTOM_RIGHT:  x = panel_w - img_w;        y = panel_h - img_h;         break;
  }
  if (x & 1) --x;   // keep 4bpp nibble alignment
  *out_x = x; *out_y = y;
}

static const char* fit_name(DisplayFit f) {
  switch (f) {
    case FIT_ORIGINAL: return "ORIGINAL";
    case FIT_CONTAIN:  return "CONTAIN";
    case FIT_SCALE:    return "SCALE";
  }
  return "?";
}
static const char* anchor_name(DisplayAnchor a) {
  switch (a) {
    case ANCHOR_TOP_LEFT:      return "TOP_LEFT";
    case ANCHOR_TOP_CENTER:    return "TOP_CENTER";
    case ANCHOR_TOP_RIGHT:     return "TOP_RIGHT";
    case ANCHOR_MIDDLE_LEFT:   return "MIDDLE_LEFT";
    case ANCHOR_CENTER:        return "CENTER";
    case ANCHOR_MIDDLE_RIGHT:  return "MIDDLE_RIGHT";
    case ANCHOR_BOTTOM_LEFT:   return "BOTTOM_LEFT";
    case ANCHOR_BOTTOM_CENTER: return "BOTTOM_CENTER";
    case ANCHOR_BOTTOM_RIGHT:  return "BOTTOM_RIGHT";
  }
  return "?";
}

// ----- main pipeline ----------------------------------------------------------
static void pack_4bpp_in_place(uint8_t* idx, int W, int H) {
  for (int y = 0; y < H; ++y) {
    const uint8_t* src = idx + (size_t)y * W;
    uint8_t* dst       = idx + (size_t)y * (W / 2);
    for (int x = 0; x < W; x += 2) {
      const uint8_t a = src[x]     & 0xF;
      const uint8_t b = src[x + 1] & 0xF;
      dst[x >> 1] = (uint8_t)((a << 4) | b);
    }
  }
}

// Decodes one image, dithers it and pushes it into the 4bpp sprite.
// The sprite MUST already be cleared (fillSprite) by the caller when the image
// does not cover the whole panel.
static bool show_image_on_panel(RgbImage* img) {
  // 1) Decide the final on-panel size from the source size + fit mode.
  int W, H;
  compute_target_size(img->width, img->height, DISPLAY_FIT, DISPLAY_SCALE,
                      PANEL_W, PANEL_H, &W, &H);
  int x, y;
  compute_anchor_xy(W, H, DISPLAY_ANCHOR, PANEL_W, PANEL_H, &x, &y);
  LOG.printf("[layout] src=%dx%d  fit=%s  anchor=%s  scale=%.3f\n",
             img->width, img->height, fit_name(DISPLAY_FIT),
             anchor_name(DISPLAY_ANCHOR), DISPLAY_SCALE);
  LOG.printf("[layout] -> target=%dx%d  at (%d,%d)  panel=%dx%d\n",
             W, H, x, y, PANEL_W, PANEL_H);
  if (W > PANEL_W || H > PANEL_H) {
    LOG.printf("[layout] target exceeds panel by (%d,%d) px -- overflow will be cropped\n",
               W - PANEL_W, H - PANEL_H);
  }

  // 2) Resize if needed. resize_image() allocates a new PSRAM buffer for the
  //    target and then frees the old one, so peak memory briefly holds both.
  if (W != img->width || H != img->height) {
    const size_t need_kb = (size_t)W * H * 3 / 1024;
    LOG.printf("[layout] resizing %dx%d -> %dx%d  (needs +%lu kB temporarily)\n",
               img->width, img->height, W, H, (unsigned long)need_kb);
    log_mem("before resize");
    if (!resize_image(img, W, H)) {
      LOG.println("[layout] resize failed (likely OOM) -- skipping this image");
      LOG.println("[layout]   try smaller photos, DITHER_BAYER8, or FIT_CONTAIN");
      image_free(img);
      return false;
    }
    log_mem("after resize");
  }

  if ((W & 1) || (H & 1)) {
    LOG.println(TAG " image size must be even on both axes for 4bpp pushImage");
    image_free(img);
    return false;
  }
  const size_t npx = (size_t)W * H;

  // 3) Dither into a 1-byte/pixel index buffer (raw E6 codes).
  log_mem("before idx malloc");
  LOG.printf(TAG " allocating index buf: %lu kB\n", (unsigned long)(npx / 1024));
  uint8_t* idx = (uint8_t*)ps_malloc(npx);
  if (!idx) idx = (uint8_t*)malloc(npx);
  if (!idx) {
    LOG.println(TAG " OOM idx -- skipping this image");
    image_free(img);
    return false;
  }

  static const char* kDitherNames[] = {"NONE", "BAYER8", "FS", "JARVIS", "ATKINSON", "BN", "YLO"};
  LOG.printf(TAG " dithering E6 with %s, gamma=%.2f ...\n",
             kDitherNames[(int)DITHER_METHOD], DITHER_GAMMA);
  const uint32_t t0 = millis();
  if (!dither_image(img->pixels, W, H, PAL_E6, DITHER_METHOD,
                    DITHER_GAMMA, /*invert=*/false, idx)) {
    LOG.println(TAG " dither failed -- skipping this image");
    free(idx);
    image_free(img);
    return false;
  }
  LOG.printf(TAG " dither done in %lu ms\n", (unsigned long)(millis() - t0));

  // 4) Free the RGB888 source -- we don't need it anymore -- then pack to 4bpp.
  LOG.println(TAG " freeing RGB888 source");
  image_free(img);
  log_mem("after RGB888 freed");

  pack_4bpp_in_place(idx, W, H);

  // 5) Push at the computed anchor location. pushImage clips automatically.
  LOG.printf(TAG " pushImage %dx%d at (%d,%d) -> sprite\n", W, H, x, y);
  epaper.pushImage(x, y, W, H, (uint16_t*)idx);
  LOG.println(TAG " update() -- panel refresh starts");
  LOG.flush();
  const uint32_t t1 = millis();
  epaper.update();
  LOG.printf(TAG " update done in %lu ms\n", (unsigned long)(millis() - t1));
  free(idx);
  return true;
}

// ----- SD directory scan ------------------------------------------------------
static bool is_image_file(const String& name) {
  String low = name;
  low.toLowerCase();
  return low.endsWith(".jpg") || low.endsWith(".jpeg") ||
         low.endsWith(".bmp") || low.endsWith(".png");
}

static String join_path(const char* dir, const String& name) {
  String out(dir);
  if (!out.endsWith("/")) out += "/";
  out += name;
  return out;
}

static size_t scan_images(const char* dirPath) {
  g_files.clear();

  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    LOG.printf("[scan] '%s' is not a directory\n", dirPath);
    if (dir) dir.close();
    return 0;
  }

  File entry = dir.openNextFile();
  while (entry && g_files.size() < MAX_FILES) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      String full = name.startsWith("/") ? name : join_path(dirPath, name);

      if (!SD.exists(full.c_str())) {
        String alt = join_path(dirPath, name);
        if (SD.exists(alt.c_str())) full = alt;
      }

      if (is_image_file(full)) g_files.push_back(full);
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();

  std::sort(g_files.begin(), g_files.end());

  LOG.printf("[scan] '%s': %u image(s)\n", dirPath, (unsigned)g_files.size());
  for (size_t i = 0; i < g_files.size(); ++i) {
    LOG.printf("   %2u. %s\n", (unsigned)(i + 1), g_files[i].c_str());
  }
  LOG.flush();
  return g_files.size();
}

static bool show_file(const String& path) {
  LOG.println();
  LOG.println("----------------------------------------------");
  LOG.printf("[slide] file: %s\n", path.c_str());
  log_mem("before load");

  RgbImage img;
  if (!load_image_from_sd(path.c_str(), /*target_w=*/0, /*target_h=*/0, &img)) {
    LOG.printf("[slide] load failed: %s\n", path.c_str());
    LOG.println("[slide]   common causes: photo larger than PSRAM budget, "
                "progressive JPEG, or a damaged file");
    return false;
  }
  LOG.printf("[slide] decoded %dx%d (%lu kB in PSRAM)\n",
             img.width, img.height,
             (unsigned long)((size_t)img.width * img.height * 3 / 1024));
  log_mem("after decode");

  // Clear the sprite first: images smaller than the panel would otherwise keep
  // the previous slide's pixels in the margins.
  // E6 code 0 == white. Do NOT use TFT_WHITE here: that is 0xFFFF and the 4bpp
  // sprite keeps only the low nibble (0xF == black).
  epaper.fillSprite(E6_WHITE);

  const bool ok = show_image_on_panel(&img);
  image_free(&img);                 // no-op if show_image_on_panel already freed
  return ok;
}

// ----- page-turn keys ---------------------------------------------------------
// Update the edge detectors without emitting an event. Called after a blocking
// slide so a key held down during the refresh does not immediately page again.
static void sync_page_keys() {
  g_nextKeyWasDown = (digitalRead(PIN_KEY_NEXT) == LOW);
  g_prevKeyWasDown = (digitalRead(PIN_KEY_PREV) == LOW);

  // If the upload key is still held when a slide finishes, restart its hold
  // timer so "press through the refresh and keep holding" still works.
  const bool up = (digitalRead(PIN_KEY_UPLOAD) == LOW);
  g_uploadWasDown   = up;
  g_uploadLongFired = false;
  g_uploadDownMs    = up ? millis() : 0;
}

static void init_page_keys() {
  pinMode(PIN_KEY0, INPUT_PULLUP);
  pinMode(PIN_KEY1, INPUT_PULLUP);
  pinMode(PIN_KEY2, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  // Sampling this right after reset is what "hold the left key at power-on"
  // means; setup() acts on it once the SD card is mounted.
  g_bootHoldWanted = (digitalRead(PIN_KEY_UPLOAD) == LOW);
  sync_page_keys();
}

// Short non-blocking beep: tone() queues the note to a small LEDC task and
// returns immediately, so the e-paper refresh is not delayed.
static void beep(unsigned int freq_hz) {
  if (!BUZZER_FEEDBACK) return;
  tone(PIN_BUZZER, freq_hz, BEEP_MS);
}

// ----- Wi-Fi upload mode entry -------------------------------------------------
// Peak PSRAM per pixel for the upload page's size budget. Every method now
// needs 3 (RGB) + 1 (index) = 4: the error rows are a small ring buffer, so
// error diffusion no longer costs the whole image.
static uint8_t dither_bytes_per_px() {
  // RGB (3) + quantized index (1). The diffusion error rows are a ring buffer
  // of a few KB, not a per-pixel allocation, so every method costs the same.
  (void)DITHER_METHOD;
  return 4;
}

// Never returns: upload mode reboots the board when it is left, so setup() runs
// again and rescans the card.
// ----- calibration mode -------------------------------------------------------
// One-off measurement helper. Set CALIBRATION_MODE = true, flash, and the board
// shows the six inks one at a time, full screen. The refresh takes ~30 s; when
// it finishes the buzzer beeps twice, and that is the moment to photograph.
// RIGHT key advances to the next ink.
//
// Photograph all six under the light you normally view the frame in (no flash,
// lock exposure/white balance between shots, fill the frame with the panel).
// The photos then replace kE6Rgb in dither.cpp with what the panel really
// reflects -- kE6Rgb is currently a guess based on web colours.
static constexpr bool CALIBRATION_MODE = false;

static const char* const kCalNames[6] = {"WHITE", "GREEN", "RED", "YELLOW", "BLUE", "BLACK"};
static const uint16_t    kCalCodes[6] = {E6_WHITE, E6_GREEN, E6_RED, E6_YELLOW, E6_BLUE, E6_BLACK};

static void run_calibration() {
  int idx = 0;
  for (;;) {
    LOG.println();
    LOG.printf("[cal] swatch %d/6: %s (code 0x%X)\n",
               idx + 1, kCalNames[idx], (unsigned)kCalCodes[idx]);
    LOG.println("[cal] refreshing, ~30 s ...");
    LOG.flush();

    epaper.fillSprite(kCalCodes[idx]);
    epaper.update();

    LOG.println("[cal] READY -- photograph now; RIGHT key = next swatch");
    LOG.flush();
    beep(1500);
    delay(90);
    beep(2000);

    bool wasDown = (digitalRead(PIN_KEY_NEXT) == LOW);
    for (;;) {
      const bool down = (digitalRead(PIN_KEY_NEXT) == LOW);
      if (down && !wasDown) {
        beep(1200);
        delay(300);
        break;
      }
      wasDown = down;
      delay(20);
    }
    idx = (idx + 1) % 6;
  }
}

static void enter_upload_mode(const char* why) {
  LOG.println();
  LOG.printf("[mode] Wi-Fi upload mode (%s)\n", why);
  LOG.printf("[mode] AP '%s'  ->  http://192.168.4.1\n", AP_SSID);
  LOG.flush();
  beep(1600);
  delay(70);
  beep(2100);

  upload_mode::Config cfg;
  cfg.image_dir    = IMAGE_DIR;
  cfg.ssid         = AP_SSID;
  cfg.password     = AP_PASSWORD;
  cfg.exit_key_pin = PIN_KEY_UPLOAD;
  cfg.buzzer_pin   = PIN_BUZZER;
  cfg.exit_hint    = "HOLD LEFT KEY 2s TO EXIT";
  cfg.panel_w      = PANEL_W;
  cfg.panel_h      = PANEL_H;
  cfg.sprite_bytes = (uint32_t)PANEL_W * (uint32_t)PANEL_H / 2;
  cfg.bytes_per_px = dither_bytes_per_px();
  cfg.budget_bytes = UPLOAD_BUDGET_BYTES;
  // E6 nibbles: 0x0 white, 0xF black, 0xD blue.
  cfg.color_bg     = E6_WHITE;
  cfg.color_fg     = E6_BLACK;
  cfg.color_muted  = E6_BLUE;
  upload_mode::run(epaper, cfg);
}

// Returns +1 for a new "next" press, -1 for a new "previous" press, 0 if none.
// Both keys at once are ignored.
static int8_t poll_page_keys() {
  const bool nextDown = (digitalRead(PIN_KEY_NEXT) == LOW);
  const bool prevDown = (digitalRead(PIN_KEY_PREV) == LOW);

  int8_t dir = 0;
  if (nextDown && !g_nextKeyWasDown && !prevDown)      dir = +1;
  else if (prevDown && !g_prevKeyWasDown && !nextDown) dir = -1;

  g_nextKeyWasDown = nextDown;
  g_prevKeyWasDown = prevDown;
  return dir;
}

// True once when the left key has been held for UPLOAD_HOLD_MS. The key has no
// page-turn role, so a short tap simply does nothing.
static bool upload_hold_requested() {
  const bool down = (digitalRead(PIN_KEY_UPLOAD) == LOW);

  if (down && !g_uploadWasDown) {
    g_uploadDownMs    = millis();
    g_uploadLongFired = false;
  }

  bool fire = false;
  if (down && !g_uploadLongFired &&
      (uint32_t)(millis() - g_uploadDownMs) >= UPLOAD_HOLD_MS) {
    g_uploadLongFired = true;
    fire = true;
  }
  if (!down) g_uploadDownMs = 0;

  g_uploadWasDown = down;
  return fire;
}

// Shows g_files[idx] and updates the auto-play cursor + timer.
static bool show_slide(size_t idx, const char* why) {
  const size_t n = g_files.size();
  if (n == 0) return false;
  idx %= n;

  LOG.printf("\n[slide] (%s) %u/%u\n", why, (unsigned)(idx + 1), (unsigned)n);

  const bool ok = show_file(g_files[idx]);
  sync_page_keys();   // consume any key held during the blocking refresh

  if (ok) {
    g_current     = idx;
    g_haveCurrent = true;
    g_next        = (idx + 1) % n;
    g_lastShow    = millis();
  } else {
    g_next     = (idx + 1) % n;
    g_lastShow = millis() - SLIDE_INTERVAL_MS + 5000;
  }
  return ok;
}

// -----------------------------------------------------------------------------
void setup() {
  LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
  delay(2500);
  LOG.println();
  LOG.println("==============================================");
  LOG.println("  reTerminal E1004 -- SD Slideshow (6-color)");
  LOG.println("==============================================");
  LOG.printf("[sys] chip      : ESP32-S3 @ %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
  LOG.printf("[sys] PSRAM size: %lu kB\n", (unsigned long)(ESP.getPsramSize() / 1024));
  if (ESP.getPsramSize() == 0) {
    LOG.println("[sys] !!! PSRAM is 0 kB -- enable Tools > PSRAM > OPI PSRAM in the IDE !!!");
  }
  LOG.printf("[sys] panel     : %d x %d\n", PANEL_W, PANEL_H);
  LOG.printf("[sys] image dir : '%s'\n", IMAGE_DIR);
  LOG.printf("[sys] interval  : %lu s\n", (unsigned long)(SLIDE_INTERVAL_MS / 1000));
  LOG.flush();

  pinMode(PIN_SD_EN, OUTPUT);
  digitalWrite(PIN_SD_EN, HIGH);
  pinMode(PIN_SD_DET, INPUT_PULLUP);
  init_page_keys();
  LOG.printf("[key] next=GPIO%d  prev=GPIO%d  upload=GPIO%d  (active LOW)\n",
             PIN_KEY_NEXT, PIN_KEY_PREV, PIN_KEY_UPLOAD);
  delay(50);
  const int sd_det = digitalRead(PIN_SD_DET);
  LOG.printf("[sd] SD_DET (GPIO%d) reads %s -- %s\n", PIN_SD_DET,
             sd_det ? "HIGH" : "LOW",
             sd_det ? "no card detected (or DET pin floating)" : "card present");
  LOG.flush();

  LOG.println(TAG " EPaper begin ...");
  epaper.begin();

  if (CALIBRATION_MODE) run_calibration();   // never returns

  // E6 code 0 is white (see the note above show_file).
  epaper.fillSprite(E6_WHITE);
  epaper.update();
  LOG.println(TAG " EPaper cleared to white");
  log_mem("after epaper.begin");

  // Setup523 already declares TFT_MISO=8, so the shared SPI bus is correctly
  // initialized for SD on E1004. Re-init explicitly to be safe.
  SPIClass& spi = epaper.getSPIinstance();
  spi.end();
  spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, /*ss=*/-1);
  LOG.printf(TAG " SPI bus re-init: SCK=%d MISO=%d MOSI=%d (for SD)\n",
             PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI);

  LOG.println(TAG " SD.begin (shares HSPI with EPaper) ...");
  if (!SD.begin(PIN_SD_CS, spi)) {
    LOG.println(TAG " SD.begin FAILED -- slideshow disabled");
    LOG.println(TAG "   - Is the card inserted and formatted as FAT32?");
    LOG.printf (TAG "   - Is SD_EN (GPIO%d) actually wired to power the slot?\n", PIN_SD_EN);
    return;
  }
  LOG.printf("[sd] mounted; card size = %llu MB\n",
             (unsigned long long)(SD.cardSize() / (1024ULL * 1024ULL)));
  list_sd_root();

  // Right key held at power-on (or still held now) -> Wi-Fi upload mode.
  if (g_bootHoldWanted || digitalRead(PIN_KEY_UPLOAD) == LOW) {
    enter_upload_mode("left key held at boot");
  }

  // Prefer IMAGE_DIR; fall back to the card root if it does not exist.
  size_t count = scan_images(IMAGE_DIR);
  if (count == 0 && strcmp(IMAGE_DIR, "/") != 0) {
    LOG.printf("[scan] no images in '%s' -- falling back to '/'\n", IMAGE_DIR);
    count = scan_images("/");
  }
  if (count == 0) {
    LOG.println(TAG " no supported images found -- opening Wi-Fi upload mode");
    LOG.println(TAG "   supported: .jpg .jpeg .bmp .png");
    LOG.flush();
    enter_upload_mode("no images on card");
  }

  LOG.println(TAG " showing first slide ...");
  show_slide(0, "boot");
  LOG.printf("[slide] %u image(s) queued; auto-advance every %lu s "
             "(right key = next, middle key = previous)\n",
             (unsigned)g_files.size(), (unsigned long)(SLIDE_INTERVAL_MS / 1000));
  LOG.printf("[key] hold left key %lu s for Wi-Fi upload mode (AP '%s')\n",
             (unsigned long)(UPLOAD_HOLD_MS / 1000), AP_SSID);
  log_mem("after first slide");
}

void loop() {
  if (g_files.empty()) {
    delay(1000);
    return;
  }

  const size_t n = g_files.size();

  // Right key held -> Wi-Fi upload mode (never returns).
  if (upload_hold_requested()) {
    enter_upload_mode("left key held");
  }

  int8_t keyDir = 0;
  if ((uint32_t)(millis() - g_lastKeyMs) >= KEY_LOCKOUT_MS) {
    keyDir = poll_page_keys();
  } else {
    poll_page_keys();   // keep the edge detectors current, ignore the event
  }

  if (keyDir > 0) {
    g_lastKeyMs = millis();
    const size_t target = g_haveCurrent ? (g_current + 1) % n : g_next % n;
    LOG.println("[key] NEXT");
    beep(BEEP_NEXT_HZ);
    show_slide(target, "key next");
    return;
  }
  if (keyDir < 0) {
    g_lastKeyMs = millis();
    const size_t target = g_haveCurrent ? (g_current + n - 1) % n : g_next % n;
    LOG.println("[key] PREV");
    beep(BEEP_PREV_HZ);
    show_slide(target, "key prev");
    return;
  }

  // Non-blocking auto-advance timer: keep each slide for SLIDE_INTERVAL_MS.
  if ((uint32_t)(millis() - g_lastShow) < SLIDE_INTERVAL_MS) {
    delay(20);
    return;
  }

  show_slide(g_next % n, "auto");
}