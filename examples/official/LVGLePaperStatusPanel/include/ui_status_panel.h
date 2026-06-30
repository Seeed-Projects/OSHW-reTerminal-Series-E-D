#pragma once

#include <lvgl.h>

void ui_status_panel_create(void);
void ui_status_panel_set_status(const char *status, const char *network, int battery_percent);
