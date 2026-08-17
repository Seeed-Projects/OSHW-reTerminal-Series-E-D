#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <Arduino.h>
#include "esp_event.h"
#include <vector>
#include "utils/mem_malloc.h"

enum class ProvisioningOrigin : uint8_t
{
    Unknown,
    WebPortal,
    BleAt,
    AutoConnect,
};


typedef struct {
    char ssid[33];
    wifi_auth_mode_t authmode;
    int8_t  rssi;
    int32_t channel;
} app_wifi_ap_record_t;

struct CachedAPInfo {
    char ssid[33];
    uint8_t bssid[6];
    int32_t channel;
    int8_t rssi;
    wifi_auth_mode_t authmode;
};

typedef struct {
    uint16_t number;
    app_wifi_ap_record_t *ap_records;
} app_wifi_scan_done_args_t;

typedef struct {
    char ssid[33];
    char password[65];
    ProvisioningOrigin origin;
} app_wifi_connect_args_t;

typedef struct {
    char ssid[33];
    int reason_code; // 0 for success, non-zero for failure
} wifi_connect_result_t;

enum {
    APP_WIFI_EVENT_START_AUTOCONNECT,
    APP_WIFI_EVENT_START_PORTAL,
    APP_WIFI_EVENT_SCAN,
    APP_WIFI_EVENT_SCAN_DONE,
    APP_WIFI_EVENT_CONNECT,
    APP_WIFI_EVENT_CONNECTED,
    APP_WIFI_EVENT_DISCONNECTED,
    APP_WIFI_EVENT_PORTAL_DONE,
    APP_WIFI_EVENT_CONNECT_FAILED,
    APP_MQTT_DISCONNECTED,
};

const std::vector<CachedAPInfo, util::psram_allocator<CachedAPInfo>>& app_wifi_get_scan_cache();

void app_wifi_init();
void app_wifi_request_autoconnect(bool manual);

bool get_portal_status();
bool app_wifi_is_provisioning();
ProvisioningOrigin app_wifi_get_origin();
void app_wifi_clear_origin();

#endif // APP_WIFI_H
