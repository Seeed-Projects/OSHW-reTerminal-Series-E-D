#include "ui_status_panel.h"

#include <cstdio>

#ifndef EPAPER_LVGL_HOR_RES
#define EPAPER_LVGL_HOR_RES 800
#endif

#ifndef EPAPER_LVGL_VER_RES
#define EPAPER_LVGL_VER_RES 480
#endif

#define CARD_CONTENT_OFFSET 32

static lv_obj_t *status_value;
static lv_obj_t *network_value;
static lv_obj_t *battery_bar;
static lv_obj_t *battery_value;
static lv_obj_t *battery_note;

static int32_t max_i32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static lv_obj_t *create_card(lv_obj_t *parent, const char *title, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t accent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 0, 0);
    lv_obj_set_style_pad_all(card, 16, 0);

    lv_obj_t *accent_bar = lv_obj_create(card);
    lv_obj_set_size(accent_bar, 8, h - 32);
    lv_obj_set_pos(accent_bar, 0, 16);
    lv_obj_set_style_bg_color(accent_bar, accent, 0);
    lv_obj_set_style_border_width(accent_bar, 0, 0);
    lv_obj_set_style_radius(accent_bar, 0, 0);
    lv_obj_set_style_pad_all(accent_bar, 0, 0);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, CARD_CONTENT_OFFSET, 0);

    return card;
}

void ui_status_panel_create(void)
{
    const int32_t screen_width = EPAPER_LVGL_HOR_RES;
    const int32_t screen_height = EPAPER_LVGL_VER_RES;
    const bool is_landscape = screen_width >= screen_height;
    const int32_t margin = max_i32(32, screen_width / 20);
    const int32_t gap = max_i32(20, screen_width / 40);
    const int32_t title_y = max_i32(24, screen_height / 16);
    const int32_t content_y = max_i32(96, screen_height / 5);

    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Seeed ePaper LVGL Panel");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, title_y);

    int32_t status_x = margin;
    int32_t status_y = content_y;
    int32_t status_w = 360;
    int32_t status_h = 150;
    int32_t network_x = margin;
    int32_t network_y = content_y + status_h + gap;
    int32_t network_w = status_w;
    int32_t network_h = status_h;
    int32_t battery_x = margin;
    int32_t battery_y = network_y + network_h + gap;
    int32_t battery_w = status_w;
    int32_t battery_h = status_h;

    if (is_landscape) {
        status_w = (screen_width - (margin * 2) - gap) / 2;
        network_w = status_w;
        status_h = max_i32(120, screen_height / 3);
        network_h = status_h;
        status_x = margin;
        status_y = content_y;
        network_x = status_x + status_w + gap;
        network_y = status_y;
        battery_x = margin;
        battery_y = status_y + status_h + gap;
        battery_w = screen_width - (margin * 2);
        battery_h = screen_height - battery_y - margin;
    } else {
        const int32_t card_w = screen_width - (margin * 2);
        const int32_t card_h = max_i32(180, (screen_height - content_y - margin - (gap * 2)) / 3);
        status_w = card_w;
        network_w = card_w;
        battery_w = card_w;
        status_h = card_h;
        network_h = card_h;
        battery_h = card_h;
        status_x = margin;
        status_y = content_y;
        network_x = margin;
        network_y = status_y + status_h + gap;
        battery_x = margin;
        battery_y = network_y + network_h + gap;
    }

    lv_obj_t *status_card = create_card(screen, "Device", status_x, status_y, status_w, status_h, lv_palette_main(LV_PALETTE_RED));
    status_value = lv_label_create(status_card);
    lv_label_set_text(status_value, "Ready");
    lv_obj_set_style_text_font(status_value, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(status_value, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(status_value, LV_ALIGN_BOTTOM_LEFT, CARD_CONTENT_OFFSET, -8);

    lv_obj_t *network_card = create_card(screen, "Network", network_x, network_y, network_w, network_h, lv_palette_main(LV_PALETTE_BLUE));
    network_value = lv_label_create(network_card);
    lv_label_set_text(network_value, "Offline");
    lv_obj_set_style_text_font(network_value, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(network_value, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(network_value, LV_ALIGN_BOTTOM_LEFT, CARD_CONTENT_OFFSET, -8);

    lv_obj_t *battery_card = create_card(screen, "Battery Demo", battery_x, battery_y, battery_w, battery_h, lv_palette_main(LV_PALETTE_GREEN));
    battery_bar = lv_bar_create(battery_card);
    lv_obj_set_size(battery_bar, max_i32(220, battery_w - 252), 28);
    lv_obj_align(battery_bar, LV_ALIGN_BOTTOM_LEFT, CARD_CONTENT_OFFSET, -12);
    lv_bar_set_range(battery_bar, 0, 100);
    lv_bar_set_value(battery_bar, 76, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(battery_bar, lv_palette_lighten(LV_PALETTE_GREEN, 4), LV_PART_MAIN);
    lv_obj_set_style_bg_color(battery_bar, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);

    battery_value = lv_label_create(battery_card);
    lv_label_set_text(battery_value, "76% Demo");
    lv_obj_set_style_text_font(battery_value, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(battery_value, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(battery_value, LV_ALIGN_BOTTOM_RIGHT, 0, -4);

    battery_note = lv_label_create(battery_card);
    lv_label_set_text(battery_note, "Sample value");
    lv_obj_set_style_text_font(battery_note, &lv_font_montserrat_14, 0);
    lv_obj_align(battery_note, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, "Static LVGL UI rendered on ePaper");
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_14, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -18);
}

void ui_status_panel_set_status(const char *status, const char *network, int battery_percent)
{
    if (battery_percent < 0) {
        battery_percent = 0;
    }
    if (battery_percent > 100) {
        battery_percent = 100;
    }

    lv_label_set_text(status_value, status);
    lv_label_set_text(network_value, network);
    lv_bar_set_value(battery_bar, battery_percent, LV_ANIM_OFF);

    static char battery_text[16];
    snprintf(battery_text, sizeof(battery_text), "%d%% Demo", battery_percent);
    lv_label_set_text(battery_value, battery_text);
    lv_label_set_text(battery_note, "Sample value");
}
