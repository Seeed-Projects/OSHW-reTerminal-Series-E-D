#ifndef APP_DOWNLOAD_H
#define APP_DOWNLOAD_H

#include <Arduino.h>

void app_download_init();

void app_download_set_manifest_url(const String &url, const String &version, const char *only_id = nullptr);
bool app_download_refresh_image(const char *id);
bool app_download_is_same_manifest_in_progress(const char *version, const char *url);


#endif // APP_DOWNLOAD_H
