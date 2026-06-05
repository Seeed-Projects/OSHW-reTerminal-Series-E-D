#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Preferences.h>
#include "TFT_eSPI.h"
#include "dither.h"
#include "image_loader.h"

#ifndef EPAPER_ENABLE
#error "This example requires Setup520_Seeed_reTerminal_E1001 -- check driver.h selects BOARD_SCREEN_COMBO 520"
#endif

EPaper epaper;

static constexpr int    PIN_SD_SCK   = 7;
static constexpr int    PIN_SD_MISO  = 8;
static constexpr int    PIN_SD_MOSI  = 9;
static constexpr int    PIN_SD_CS    = 14;
static constexpr int    PIN_SD_EN    = 16;
static constexpr int    PIN_SD_DET   = 15;
static constexpr int    PIN_DBG_RX   = 44;
static constexpr int    PIN_DBG_TX   = 43;
#define LOG       Serial1
#define TAG       "[e1001-bw]"

// =============================================================================
// USER CONFIGURATION
// =============================================================================

enum DisplayAnchor {
  ANCHOR_TOP_LEFT, ANCHOR_TOP_CENTER, ANCHOR_TOP_RIGHT,
  ANCHOR_MIDDLE_LEFT, ANCHOR_CENTER, ANCHOR_MIDDLE_RIGHT,
  ANCHOR_BOTTOM_LEFT, ANCHOR_BOTTOM_CENTER, ANCHOR_BOTTOM_RIGHT,
};

enum DisplayFit { FIT_ORIGINAL, FIT_CONTAIN, FIT_SCALE };

static constexpr const char* DEFAULT_IMAGE_FILE = "/img/demo.jpg";
static constexpr DitherMethod DEFAULT_DITHER = DITHER_FS;
static constexpr float DEFAULT_GAMMA = 1.0f;
static constexpr bool DEFAULT_INVERT = false;
static constexpr DisplayAnchor DEFAULT_ANCHOR = ANCHOR_CENTER;
static constexpr DisplayFit DEFAULT_FIT = FIT_SCALE;
static constexpr float DEFAULT_SCALE = 0.7f;

static String imagePath = DEFAULT_IMAGE_FILE;
static DitherMethod ditherMethod = DEFAULT_DITHER;
static float ditherGamma = DEFAULT_GAMMA;
static bool ditherInvert = DEFAULT_INVERT;
static DisplayAnchor displayAnchor = DEFAULT_ANCHOR;
static DisplayFit displayFit = DEFAULT_FIT;
static float displayScale = DEFAULT_SCALE;

// Reads a float stored as a 4-byte NVS blob.
// 读取以 4 字节 blob 保存的浮点数配置。
static float read_float_config(Preferences& prefs, const char* key, float default_value) {
  float value = default_value;
  if (prefs.getBytesLength(key) == sizeof(value)) {
    prefs.getBytes(key, &value, sizeof(value));
  }
  return value;
}

// Converts a stored integer into a valid dithering enum value.
// 把保存的整数转换成有效的抖动算法枚举值。
static DitherMethod read_dither_config(int value) {
  switch (value) {
    case DITHER_NONE:
    case DITHER_BAYER8:
    case DITHER_FS:
    case DITHER_JARVIS:
    case DITHER_ATKINSON:
      return (DitherMethod)value;
    default:
      return DEFAULT_DITHER;
  }
}

// Converts a stored integer into a valid image anchor enum value.
// 把保存的整数转换成有效的图片定位枚举值。
static DisplayAnchor read_anchor_config(int value) {
  if (value >= ANCHOR_TOP_LEFT && value <= ANCHOR_BOTTOM_RIGHT) {
    return (DisplayAnchor)value;
  }
  return DEFAULT_ANCHOR;
}

// Converts a stored integer into a valid image fit enum value.
// 把保存的整数转换成有效的图片缩放模式枚举值。
static DisplayFit read_fit_config(int value) {
  if (value >= FIT_ORIGINAL && value <= FIT_SCALE) {
    return (DisplayFit)value;
  }
  return DEFAULT_FIT;
}

// Loads user configuration from the NVS namespace named "config".
// 从名为 "config" 的 NVS 命名空间读取用户配置。
static void loadConfig() {
  Preferences prefs;
  if (!prefs.begin("config", true)) {
    LOG.println(TAG " config open failed; using defaults");
    return;
  }

  imagePath = prefs.getString("imagePath", DEFAULT_IMAGE_FILE);
  // The ESP32 SD/VFS layer requires absolute paths; prepend "/" if missing.
  // ESP32 的 SD/VFS 层要求绝对路径；缺少前导 "/" 时自动补上。
  if (imagePath.length() > 0 && imagePath[0] != '/') imagePath = "/" + imagePath;
  ditherMethod = read_dither_config(prefs.getInt("dither", (int)DEFAULT_DITHER));
  ditherGamma = read_float_config(prefs, "gamma", DEFAULT_GAMMA);
  ditherInvert = prefs.getUChar("invert", DEFAULT_INVERT ? 1 : 0) != 0;
  displayAnchor = read_anchor_config(prefs.getInt("anchor", (int)DEFAULT_ANCHOR));
  displayFit = read_fit_config(prefs.getInt("fitMode", (int)DEFAULT_FIT));
  displayScale = read_float_config(prefs, "scale", DEFAULT_SCALE);
  prefs.end();
  LOG.println(TAG " config loaded");
}

// =============================================================================
// (implementation below -- no need to edit)
// =============================================================================

static void log_mem(const char* tag) {
  LOG.printf("[mem] %-22s heap=%lu kB  PSRAM free=%lu/%lu kB\n", tag,
             (unsigned long)(ESP.getFreeHeap() / 1024),
             (unsigned long)(ESP.getFreePsram() / 1024),
             (unsigned long)(ESP.getPsramSize() / 1024));
  LOG.flush();
}

static void list_sd_root(int max_entries = 32) {
  File root = SD.open("/");
  if (!root || !root.isDirectory()) { LOG.println("[sd] cannot open '/'"); return; }
  LOG.println("[sd] contents of '/' :");
  int n = 0;
  File entry = root.openNextFile();
  while (entry) {
    if (entry.isDirectory()) LOG.printf("   <DIR>  %s\n", entry.name());
    else LOG.printf("   %7lu B  %s\n", (unsigned long)entry.size(), entry.name());
    entry.close();
    if (++n >= max_entries) { LOG.printf("   ... (truncated at %d)\n", max_entries); break; }
    entry = root.openNextFile();
  }
  if (n == 0) LOG.println("   (empty)");
  root.close(); LOG.flush();
}

static void compute_target_size(int src_w, int src_h, DisplayFit fit, float scale,
                                int panel_w, int panel_h, int* out_w, int* out_h) {
  switch (fit) {
    case FIT_ORIGINAL: *out_w = src_w; *out_h = src_h; break;
    case FIT_CONTAIN: {
      double s = (double)panel_w/src_w; if ((double)panel_h/src_h < s) s = (double)panel_h/src_h;
      if (s > 1.0) s = 1.0;
      *out_w = (int)(src_w*s+0.5); *out_h = (int)(src_h*s+0.5); break;
    }
    case FIT_SCALE: *out_w = (int)(src_w*scale+0.5); *out_h = (int)(src_h*scale+0.5); break;
  }
  if (*out_w & 7) *out_w &= ~7;
  if (*out_w < 8) *out_w = 8;
  if (*out_h < 1) *out_h = 1;
}

static void compute_anchor_xy(int img_w, int img_h, DisplayAnchor a,
                              int panel_w, int panel_h, int* out_x, int* out_y) {
  int x = 0, y = 0;
  switch (a) {
    case ANCHOR_TOP_LEFT:      x=0;                     y=0;                    break;
    case ANCHOR_TOP_CENTER:    x=(panel_w-img_w)/2;     y=0;                    break;
    case ANCHOR_TOP_RIGHT:     x=panel_w-img_w;         y=0;                    break;
    case ANCHOR_MIDDLE_LEFT:   x=0;                     y=(panel_h-img_h)/2;    break;
    case ANCHOR_CENTER:        x=(panel_w-img_w)/2;     y=(panel_h-img_h)/2;    break;
    case ANCHOR_MIDDLE_RIGHT:  x=panel_w-img_w;         y=(panel_h-img_h)/2;    break;
    case ANCHOR_BOTTOM_LEFT:   x=0;                     y=panel_h-img_h;        break;
    case ANCHOR_BOTTOM_CENTER: x=(panel_w-img_w)/2;     y=panel_h-img_h;        break;
    case ANCHOR_BOTTOM_RIGHT:  x=panel_w-img_w;         y=panel_h-img_h;        break;
  }
  if (x & 7) x &= ~7;
  *out_x = x; *out_y = y;
}

static bool show_image_on_panel(RgbImage* img) {
  int W, H;
  compute_target_size(img->width, img->height, displayFit, displayScale,
                      EPD_WIDTH, EPD_HEIGHT, &W, &H);
  int x, y;
  compute_anchor_xy(W, H, displayAnchor, EPD_WIDTH, EPD_HEIGHT, &x, &y);

  if (W != img->width || H != img->height) {
    if (!resize_image(img, W, H)) { LOG.println("[layout] resize OOM"); return false; }
  }
  const size_t npx = (size_t)W * H;
  uint8_t* idx = (uint8_t*)ps_malloc(npx);
  if (!idx) idx = (uint8_t*)malloc(npx);
  if (!idx) { LOG.println(TAG " OOM idx"); return false; }

  static const char* kDN[] = {"NONE","BAYER8","FS","JARVIS","ATKINSON"};
  LOG.printf(TAG " dithering BW with %s, gamma=%.2f\n", kDN[(int)ditherMethod], ditherGamma);
  if (!dither_image(img->pixels, W, H, PAL_BW, ditherMethod, ditherGamma, ditherInvert, idx)) {
    free(idx); return false;
  }
  image_free(img);

  const size_t bm_bytes = ((size_t)W+7)/8 * (size_t)H;
  uint8_t* bm = (uint8_t*)ps_malloc(bm_bytes);
  if (!bm) bm = (uint8_t*)malloc(bm_bytes);
  if (!bm) { free(idx); return false; }
  pack_1bpp_msb(idx, bm, W, H, true);
  free(idx);

  epaper.drawBitmap(x, y, bm, W, H, TFT_BLACK, TFT_WHITE);
  epaper.update();
  free(bm);
  return true;
}

void setup() {
  LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
  loadConfig();
  delay(2500);
  LOG.println("==============================================");
  LOG.println("  reTerminal E1001 -- SD Bitmap (BW)");
  LOG.println("==============================================");
  log_mem("start");

  pinMode(PIN_SD_EN, OUTPUT); digitalWrite(PIN_SD_EN, HIGH);
  pinMode(PIN_SD_DET, INPUT_PULLUP); delay(50);

  epaper.begin();
  epaper.fillScreen(TFT_WHITE);
  epaper.update();

  // UC8179 is write-only (TFT_MISO=-1 in Setup520). Re-init SPI with MISO for SD.
  SPIClass& spi = epaper.getSPIinstance();
  spi.end();
  spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, -1);

  if (!SD.begin(PIN_SD_CS, spi)) {
    LOG.println(TAG " SD.begin FAILED -- aborting"); return;
  }
  list_sd_root();

  RgbImage img;
  if (!load_image_from_sd(imagePath.c_str(), 0, 0, &img)) {
    LOG.println(TAG " load failed -- aborting"); return;
  }
  log_mem("after decode");
  show_image_on_panel(&img);
  image_free(&img);

  epaper.sleep();
  LOG.println(TAG " done.");
}

void loop() { delay(1000); }
