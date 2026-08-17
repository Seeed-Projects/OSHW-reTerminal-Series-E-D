#ifndef APP_INPUT_H
#define APP_INPUT_H

#include "APP/app_events.h"

enum app_input_event_id_t {
    APP_INPUT_EVENT_PRIMARY_ACTION,
    APP_INPUT_EVENT_NEXT_IMAGE,
    APP_INPUT_EVENT_PREV_IMAGE,
    APP_INPUT_EVENT_CLEAR_SCREEN,
    APP_INPUT_EVENT_ENTER_PROVISIONING,
    APP_INPUT_EVENT_REQUEST_CLOUD_REFRESH,
};

enum app_input_source_t {
    APP_INPUT_SOURCE_KEY0,
    APP_INPUT_SOURCE_KEY1,
    APP_INPUT_SOURCE_KEY2,
    APP_INPUT_SOURCE_TOUCH,
    APP_INPUT_SOURCE_KEY1_KEY2,
};

typedef struct {
    app_input_source_t source;
} app_input_event_data_t;

void app_input_init();

#endif // APP_INPUT_H
