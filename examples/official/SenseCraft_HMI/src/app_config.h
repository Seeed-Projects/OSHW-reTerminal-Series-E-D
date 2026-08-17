#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "APP/app_events.h"
#include "driver.h"

static const char* g_currentAppVersion = "1.1.5"; 
#define FIRMWARE_BUILD_TIMESTAMP __DATE__ " " __TIME__
#define chip_model_name "esp32s3"
#define Protocol_version "0.2"

#define LED_ALWAYS_ON 0
#define RETERMINAL_DEBUG 0
#define RETERMINAL_INFO_DEBUG 0
#define RETERMINAL_DEEPSLEEP_DISABLE 0


// --- API ---
#define HMI_TEST_ENV 0
#if HMI_TEST_ENV
    // This literal is intentionally embedded in the APP binary. The release
    // platform uses it to distinguish Test firmware from Production firmware.
    #define HMI_FIRMWARE_ENV_MARKER "@SC_HMI_FW_ENV=TEST@"
    #define API_BASE_URL "https://test-sensecraft-hmi-api.seeed.cc"
#else
    // Keep this marker format aligned with the reTerminal Sticky firmware so
    // the backend can identify firmware from both projects consistently.
    #define HMI_FIRMWARE_ENV_MARKER "@SC_HMI_FW_ENV=PROD@"
    #define API_BASE_URL "https://sensecraft-hmi-api.seeed.cc"
#endif

#define API_PATH_DEVICE_BIND "/api/v1/device/bind"
#define API_PATH_REFRESH_TOKEN "/api/v1/device/refresh_token"


// --- HTTP Client and Retry Configuration ---
#define HTTP_REQUEST_TIMEOUT_MS 6000
#define INITIAL_BIND_API_RETRY_DELAY_MS 5000
#define MAX_RETRIES 3
#define MQTT_RECONNECT_INTERVAL_MS 10000

// --- Manifest limits ---
// Keep manifest parsing bounded to avoid runaway allocation on malformed payloads.
#define MANIFEST_MAX_SIZE_BYTES (1024 * 1024) // 1 MiB

// --- Audio Configuration ---
#define SAMPLE_RATE 16000U
#define SAMPLE_BITS 16

#define PCF8563T_ADDRESS 0x51
#define SHT40_ADDRESS 0x44

// --- Root CA Certificate ---
static const uint8_t rootCACertificate[]  = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDxTCCAq2gAwIBAgIBADANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx\n"
"EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT\n"
"EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp\n"
"ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5MDkwMTAwMDAwMFoXDTM3MTIzMTIz\n"
"NTk1OVowgYMxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH\n"
"EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjExMC8GA1UE\n"
"AxMoR28gRGFkZHkgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIw\n"
"DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL9xYgjx+lk09xvJGKP3gElY6SKD\n"
"E6bFIEMBO4Tx5oVJnyfq9oQbTqC023CYxzIBsQU+B07u9PpPL1kwIuerGVZr4oAH\n"
"/PMWdYA5UXvl+TW2dE6pjYIT5LY/qQOD+qK+ihVqf94Lw7YZFAXK6sOoBJQ7Rnwy\n"
"DfMAZiLIjWltNowRGLfTshxgtDj6AozO091GB94KPutdfMh8+7ArU6SSYmlRJQVh\n"
"GkSBjCypQ5Yj36w6gZoOKcUcqeldHraenjAKOc7xiID7S13MMuyFYkMlNAJWJwGR\n"
"tDtwKj9useiciAF9n9T521NtYJ2/LOdYq7hfRvzOxBsDPAnrSTFcaUaz4EcCAwEA\n"
"AaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYE\n"
"FDqahQcQZyi27/a9BUFuIMGU2g/eMA0GCSqGSIb3DQEBCwUAA4IBAQCZ21151fmX\n"
"WWcDYfF+OwYxdS2hII5PZYe096acvNjpL9DbWu7PdIxztDhC2gV7+AJ1uP2lsdeu\n"
"9tfeE8tTEH6KRtGX+rcuKxGrkLAngPnon1rpN5+r5N9ss4UXnT3ZJE95kTXWXwTr\n"
"gIOrmgIttRD02JDHBHNA7XIloKmf7J6raBKZV8aPEjoJpL1E/QYVN8Gb5DKj7Tjo\n"
"2GTzLH4U/ALqn83/B2gX2yKQOC16jdFU8WnjXzPKej17CuPKf1855eJ1usV2GDPO\n"
"LPAvTK33sefOT6jEm0pUBsV/fdUID+Ic/n4XuKxe9tQWskMJDE32p2u0mYRlynqI\n"
"4uJEvlz36hz1\n"
"-----END CERTIFICATE-----\n";



struct BoardInfo {
    String type;
    String screen_type;
    String ssid;
    int rssi;
    int channel;
    String ip;
    String boardMac;
    String resolution;
    String img_type;
};


struct MqttConfig {
    String host;
    uint16_t port;
    String clientID;
    String username;
    String password;
    String publishTopic;
    String subscribeTopic;
};

typedef struct {
    int   activationCode;
    char  activationMsg[64];   
} ActivationEventData;

typedef struct {
    char ssid[32];
    char password[64];
} wifi_app_connect_event_data_t;

#define IMAGE_ID_MAX_LEN 128
typedef struct {
    bool apply;
    int download_progress;
    char version_id[IMAGE_ID_MAX_LEN];
    int image_index;
    int image_total;
} ctrl_image_refresh;


// --- Enumerations ---
enum DeviceReset
{
    DEVICE_DEFAULT = 0,
    DEVICE_NETWORK,
    DEVICE_BIND
};

enum SensecraftTask
{
    TaskStop = 0,
    TaskStart,
    TaskPending
};

enum view_event_id_t {
    VIEW_EVENT_SHOW_STARTUP,
    VIEW_EVENT_SHOW_IMAGE,
    VIEW_EVENT_SHOW_IMAGE_FAST,
    VIEW_EVENT_SHOW_WAITING,
    VIEW_EVENT_SHOW_ACTIVATION_CODE,
    VIEW_EVENT_SHOW_DOWNLOADING,
    VIEW_EVENT_SHOW_CLEAR,
    VIEW_EVENT_SHOW_ERROR,
    VIEW_EVENT_SHOW_LOW_BATTERY,
};

enum{
    DEVICE_ONLINE = 1,
    DEVICE_OFFLINE = 2,
    DEVICE_SLEEP = 3
};

enum{
    DOWNLOAD_EVT_JSON_DOWNLOAD,
    DOWNLOAD_EVT_JSON_PARSE,
    DOWNLOAD_EVT_IMAGE_DOWNLOAD,
    DOWNLOAD_EVT_TASK_FINISHED,
    DOWNLOAD_EVT_TASK_PAUSE,
    DOWNLOAD_EVT_TASK_FAILED,
    DOWNLOAD_EVT_TASK_CANCEL
};


enum {
    WIFI_APP_EVENT_CONNECT_START,
    WIFI_APP_EVENT_CONNECTED,
    WIFI_APP_EVENT_CONNECT_FAILED,
    WIFI_APP_EVENT_DISCONNECTED,
};

enum
{
    SENSECRAFT_EVENT_START,
    SENSECRAFT_EVENT_STOP,
    SENSECRAFT_EVENT_RESUME,
    HTTP_EVENT_DEVICE_BIND,
    MQTT_EVENT_CONNECT,
    MQTT_EVENT_SCREEN_REFRESH,
    MQTT_EVENT_IOT_REPORT,
    DEVICE_EVENT_DEEPSLEEP,

    APP_ATCMD_WIFI_SCAN,
    APP_ATCMD_WIFI_GET,
    APP_ATCMD_WIFI_CONNECT,
    APP_ATCMD_BIND,
    APP_ATCMD_BLE_RX,
    APP_ATCMD_DEVICE_INFO_BATTERY,
    APP_ATCMD_DEVICE_INFO_HUMI,
    APP_ATCMD_DEVICE_INFO_TEMP,
};

enum class WakeupClick{
    None,
    Key0Short,
    Touch,
    Key1Short,
    Key2Short,
    DoubleLong,
    TripleLong,
    Timer
};



#endif
