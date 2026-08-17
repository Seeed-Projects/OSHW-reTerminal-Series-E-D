#pragma once

#include <stdint.h>

#include "esp_event_base.h"
#include "esp_event.h"


#ifdef __cplusplus
extern "C" {
#endif


ESP_EVENT_DECLARE_BASE(VIEW_EVENT_BASE);
ESP_EVENT_DECLARE_BASE(CTRL_EVENT_BASE);
ESP_EVENT_DECLARE_BASE(WIFI_APP_EVENT_BASE);
ESP_EVENT_DECLARE_BASE(DOWNLOAD_EVENT_BASE);
ESP_EVENT_DECLARE_BASE(APP_INPUT_EVENT_BASE);

#ifdef __cplusplus
}

#endif
