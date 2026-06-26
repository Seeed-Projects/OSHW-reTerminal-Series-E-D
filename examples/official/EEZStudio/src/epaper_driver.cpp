#include "epaper_driver.h"

#include <Arduino.h>
#include "TFT_eSPI.h"
#include <lvgl.h>

#ifndef EPAPER_ENABLE
#error "EPAPER_ENABLE not defined -- check BOARD_SCREEN_COMBO in platformio.ini"
#endif

#define LOG Serial1

// Rows per LVGL partial-render band.
// LVGL buffer = EPD_WIDTH * LVGL_BUF_LINES * 2 (RGB565).
// Max ~180 KB on E1003 (1872 wide), well within PSRAM.
#define LVGL_BUF_LINES 48

static EPaper epaper;

#if EPD_COLOR_DEPTH == 1

static inline bool px_is_white(uint16_t px) {
    uint8_t r = (px >> 11) & 0x1F;
    uint8_t g = (px >> 5) & 0x3F;
    uint8_t b = px & 0x1F;
    return ((r << 1) + g + b) > 62;
}

#elif EPD_COLOR_DEPTH == 4

// 6-color ePaper palette: value -> RGB888
// 0x0=White, 0x2=Green, 0x6=Red, 0xB=Yellow, 0xD=Blue, 0xF=Black
static const uint8_t PAL_EPD[] = { 0x0, 0x2, 0x6, 0xB, 0xD, 0xF };
static const uint8_t PAL_R[]   = { 255,   0, 255, 255,   0,   0 };
static const uint8_t PAL_G[]   = { 255, 255,   0, 255,   0,   0 };
static const uint8_t PAL_B[]   = { 255,   0,   0,   0, 255,   0 };

static inline uint8_t rgb565_to_palette(uint16_t px) {
    uint8_t r5 = (px >> 11) & 0x1F;
    uint8_t g6 = (px >> 5) & 0x3F;
    uint8_t b5 = px & 0x1F;
    uint8_t r = (r5 << 3) | (r5 >> 2);
    uint8_t g = (g6 << 2) | (g6 >> 4);
    uint8_t b = (b5 << 3) | (b5 >> 2);

    uint32_t best_dist = UINT32_MAX;
    uint8_t best = 0x0F;
    for (int i = 0; i < 6; i++) {
        int32_t dr = (int32_t)r - PAL_R[i];
        int32_t dg = (int32_t)g - PAL_G[i];
        int32_t db = (int32_t)b - PAL_B[i];
        uint32_t dist = dr * dr + dg * dg + db * db;
        if (dist < best_dist) {
            best_dist = dist;
            best = PAL_EPD[i];
        }
    }
    return best;
}

#endif

static void rgb565_to_epaper(const uint16_t *src, uint8_t *dst,
                             int32_t w, int32_t h,
                             int32_t dst_x, int32_t dst_y) {
#if EPD_COLOR_DEPTH == 1
    size_t stride = EPD_WIDTH / 8;
    for (int32_t y = 0; y < h; y++) {
        size_t si = (size_t)y * w;
        size_t di = (size_t)(dst_y + y) * stride + dst_x / 8;
        for (int32_t x = 0; x < w; x += 8) {
            uint8_t byte_val = 0;
            for (int b = 0; b < 8 && (x + b) < w; b++) {
                if (px_is_white(src[si + x + b]))
                    byte_val |= (0x80 >> b);
            }
            dst[di + x / 8] = byte_val;
        }
    }
#elif EPD_COLOR_DEPTH == 4
    size_t stride = EPD_WIDTH / 2;
    for (int32_t y = 0; y < h; y++) {
        size_t si = (size_t)y * w;
        size_t di = (size_t)(dst_y + y) * stride + dst_x / 2;
        for (int32_t x = 0; x < w; x += 2) {
            uint8_t hi = rgb565_to_palette(src[si + x]);
            uint8_t lo = ((x + 1) < w)
                             ? rgb565_to_palette(src[si + x + 1])
                             : 0x00;
            dst[di + x / 2] = (hi << 4) | lo;
        }
    }
#else
#error "Unsupported EPD_COLOR_DEPTH (expected 1 or 4)"
#endif
}

static void epaper_flush_cb(lv_display_t *disp, const lv_area_t *area,
                            uint8_t *px_map) {
    int32_t x1 = area->x1, y1 = area->y1;
    int32_t w = area->x2 - x1 + 1;
    int32_t h = area->y2 - y1 + 1;

    uint8_t *fb = (uint8_t *)epaper.frameBuffer(1);
    rgb565_to_epaper((const uint16_t *)px_map, fb, w, h, x1, y1);

    if (lv_display_flush_is_last(disp)) {
        LOG.println("[EPD] update() ...");
        LOG.flush();
        uint32_t t0 = millis();
        epaper.update();
        LOG.printf("[EPD] update() done in %lu ms\n",
                   (unsigned long)(millis() - t0));
        LOG.flush();
    }

    lv_display_flush_ready(disp);
}

void epaper_init(void) {
    LOG.printf("[EPD] begin (COMBO %d) ...\n", BOARD_SCREEN_COMBO);
    LOG.flush();

    epaper.begin();
    LOG.printf("[EPD] panel %d x %d  depth %d bpp\n",
               EPD_WIDTH, EPD_HEIGHT, EPD_COLOR_DEPTH);
    LOG.flush();

    epaper.fillScreen(TFT_WHITE);
    LOG.println("[EPD] clearing ...");
    LOG.flush();
    epaper.update();
    LOG.println("[EPD] cleared");
    LOG.flush();
    delay(500);

    size_t buf_size = (size_t)EPD_WIDTH * LVGL_BUF_LINES * 2;
    uint8_t *lvgl_buf = (uint8_t *)ps_malloc(buf_size);
    if (!lvgl_buf) lvgl_buf = (uint8_t *)malloc(buf_size);
    if (!lvgl_buf) {
        LOG.println("[EPD] FATAL: cannot allocate LVGL buffer");
        LOG.flush();
        return;
    }
    LOG.printf("[EPD] LVGL buf %p  (%lu B, %d lines)\n",
               lvgl_buf, (unsigned long)buf_size, LVGL_BUF_LINES);
    LOG.flush();

    lv_display_t *disp = lv_display_create(EPD_WIDTH, EPD_HEIGHT);
    lv_display_set_flush_cb(disp, epaper_flush_cb);
    lv_display_set_buffers(disp, lvgl_buf, NULL, buf_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    LOG.printf("[EPD] LVGL display %p  (RGB565->%dbpp, PARTIAL)\n",
               disp, EPD_COLOR_DEPTH);
    LOG.flush();
}

void epaper_refresh(void) {
    lv_obj_t *scr = lv_screen_active();
    if (scr) lv_obj_invalidate(scr);
    lv_refr_now(NULL);
}
