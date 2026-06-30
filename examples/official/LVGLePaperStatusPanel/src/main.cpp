#include <Arduino.h>
#include <cstdlib>

#include "driver.h"
#include "TFT_eSPI.h"
#include <lvgl.h>
#include "ui_status_panel.h"

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

#ifndef EPAPER_LVGL_HOR_RES
#define EPAPER_LVGL_HOR_RES 800
#endif

#ifndef EPAPER_LVGL_VER_RES
#define EPAPER_LVGL_VER_RES 480
#endif

#define LVGL_BUFFER_LINES 20
#define EPAPER_MONO_THRESHOLD 180
#define COLOR_DISTANCE_WEIGHT_RED 30
#define COLOR_DISTANCE_WEIGHT_GREEN 59
#define COLOR_DISTANCE_WEIGHT_BLUE 11

static EPaper epaper;
static lv_display_t *lvgl_display;
static uint8_t *lvgl_draw_buffer;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint16_t epaper_color;
} EPaperPaletteColor;

static bool rgb565_is_dark(uint16_t color)
{
    const uint32_t red = ((color >> 11) & 0x1F) * 255 / 31;
    const uint32_t green = ((color >> 5) & 0x3F) * 255 / 63;
    const uint32_t blue = (color & 0x1F) * 255 / 31;
    const uint32_t luminance = (red * 30) + (green * 59) + (blue * 11);

    return luminance < (EPAPER_MONO_THRESHOLD * 100);
}

static void rgb565_to_rgb888(uint16_t color, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    *red = ((color >> 11) & 0x1F) * 255 / 31;
    *green = ((color >> 5) & 0x3F) * 255 / 63;
    *blue = (color & 0x1F) * 255 / 31;
}

static uint32_t color_distance(uint8_t red, uint8_t green, uint8_t blue, const EPaperPaletteColor *candidate)
{
    const int32_t red_delta = static_cast<int32_t>(red) - candidate->red;
    const int32_t green_delta = static_cast<int32_t>(green) - candidate->green;
    const int32_t blue_delta = static_cast<int32_t>(blue) - candidate->blue;

    return (red_delta * red_delta * COLOR_DISTANCE_WEIGHT_RED) +
           (green_delta * green_delta * COLOR_DISTANCE_WEIGHT_GREEN) +
           (blue_delta * blue_delta * COLOR_DISTANCE_WEIGHT_BLUE);
}

static uint16_t rgb565_to_epaper_color(uint16_t color)
{
#if defined(RETERMINAL_E1002) || defined(RETERMINAL_E1004)
    static const EPaperPaletteColor palette[] = {
        {255, 255, 255, TFT_WHITE},
        {0, 0, 0, TFT_BLACK},
        {255, 0, 0, TFT_RED},
        {255, 255, 0, TFT_YELLOW},
        {0, 255, 0, TFT_GREEN},
        {0, 0, 255, TFT_BLUE},
    };

    uint8_t red;
    uint8_t green;
    uint8_t blue;
    rgb565_to_rgb888(color, &red, &green, &blue);

    uint16_t selected_color = palette[0].epaper_color;
    uint32_t selected_distance = color_distance(red, green, blue, &palette[0]);

    for (size_t i = 1; i < sizeof(palette) / sizeof(palette[0]); i++) {
        const uint32_t distance = color_distance(red, green, blue, &palette[i]);
        if (distance < selected_distance) {
            selected_distance = distance;
            selected_color = palette[i].epaper_color;
        }
    }

    return selected_color;
#else
    return rgb565_is_dark(color) ? TFT_BLACK : TFT_WHITE;
#endif
}

static void lvgl_flush_callback(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map)
{
    const int32_t width = area->x2 - area->x1 + 1;
    const int32_t height = area->y2 - area->y1 + 1;
    const uint16_t *pixels = reinterpret_cast<const uint16_t *>(pixel_map);

    // Convert LVGL RGB565 pixels into the native ePaper sprite color.
    // 将 LVGL 的 RGB565 像素转换到墨水屏显存支持的颜色。
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            const uint16_t color = pixels[(y * width) + x];
            epaper.drawPixel(area->x1 + x, area->y1 + y, rgb565_to_epaper_color(color));
        }
    }

    lv_display_flush_ready(display);
}

static void allocate_lvgl_buffer()
{
    const size_t buffer_pixels = EPAPER_LVGL_HOR_RES * LVGL_BUFFER_LINES;
    const size_t buffer_bytes = buffer_pixels * lv_color_format_get_size(LV_COLOR_FORMAT_RGB565);

#if defined(ESP_PLATFORM)
    // Prefer PSRAM for the LVGL draw buffer on ESP32-S3.
    // ESP32-S3 上优先把 LVGL 绘制缓冲区放到 PSRAM。
    lvgl_draw_buffer = static_cast<uint8_t *>(heap_caps_malloc(buffer_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#endif

    if (lvgl_draw_buffer == nullptr) {
        lvgl_draw_buffer = static_cast<uint8_t *>(malloc(buffer_bytes));
    }

    if (lvgl_draw_buffer == nullptr) {
        Serial.println("LVGL buffer allocation failed.");
        while (true) {
            delay(1000);
        }
    }
}

static void setup_lvgl_display()
{
    allocate_lvgl_buffer();

    lvgl_display = lv_display_create(EPAPER_LVGL_HOR_RES, EPAPER_LVGL_VER_RES);
    lv_display_set_color_format(lvgl_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lvgl_display, lvgl_flush_callback);
    lv_display_set_buffers(
        lvgl_display,
        lvgl_draw_buffer,
        nullptr,
        EPAPER_LVGL_HOR_RES * LVGL_BUFFER_LINES * lv_color_format_get_size(LV_COLOR_FORMAT_RGB565),
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );
}

static void render_epaper_once()
{
    // Let LVGL render the object tree once, then refresh the physical panel.
    // 先让 LVGL 渲染一次界面树，再刷新真实墨水屏。
    lv_tick_inc(5);
    lv_timer_handler();
    epaper.update();
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("Seeed ePaper LVGL status panel starting.");

    epaper.begin();
    epaper.fillSprite(TFT_WHITE);

    lv_init();
    setup_lvgl_display();

    ui_status_panel_create();
    ui_status_panel_set_status("Ready", "Wi-Fi Standby", 76);

    render_epaper_once();
    Serial.println("LVGL status panel rendered.");
}

void loop()
{
    delay(1000);
}
