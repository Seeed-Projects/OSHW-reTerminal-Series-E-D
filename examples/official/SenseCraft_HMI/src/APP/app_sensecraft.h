#ifndef APP_SENSECRAFT_H
#define APP_SENSECRAFT_H

#include <Arduino.h>

void app_task_init();

bool app_publish_sleep_wait(uint32_t timeout_ms);

// Request a coalesced full IoT state report after a cloud-visible setting changes.
void app_sensecraft_request_iot_report();

void request_image_resource(const char *image_id = nullptr);
void request_update_timer_start();

void imgRefreshRes(const char *version, bool apply, int download_per, const char *image_id, int image_index, int image_total);

#endif // APP_SENSECRAFT_H
