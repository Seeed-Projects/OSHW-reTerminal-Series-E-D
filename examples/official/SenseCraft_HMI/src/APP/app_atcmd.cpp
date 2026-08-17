#include "APP/app_atcmd.h"
#include "APP/app_wifi.h"
#include "APP/app_ble.h"
#include "APP/app_device_info.h"
#include "APP/app_power_manager.h"

#include "esp_err.h"

#include "app_config.h"
#include "WiFi.h"

#include "ArduinoLog.h"
#include "cJSON.h"
#include <stdarg.h>
#include <vector>

#include "utils/mem_malloc.h"
#include "minirt.h"
#include "APP/app_events.h"

typedef enum
{
    AT_ERR_OK = 0,
    AT_ERR_UNKNOWN_CMD = 1,
    AT_ERR_INVALID_JSON = 2,
    AT_ERR_MISSING_PARAM = 3,
    AT_ERR_INTERNAL = 4,
} at_error_code_t;

static char at_cmd_buffer[256];
static uint16_t at_cmd_len = 0;

static EventGroupHandle_t at_event_group;
static QueueHandle_t at_cmd_queue;

const static EventBits_t AT_EVENT_WIFI_CONNECT_BIT = (1 << 0);
const static EventBits_t AT_EVENT_WIFI_SCAN_BIT = (1 << 1);
const static EventBits_t AT_EVENT_WIFI_QUERY_BIT = (1 << 2);
const static EventBits_t AT_EVENT_CMD_RECEIVED_BIT = (1 << 3);
const static EventBits_t AT_EVENT_BIND_BIT = (1 << 4);

static char g_wifi_connect_ssid[33] = {0};
static int g_wifi_connect_reason = 0;

static void __send_response(const char *format, ...);
static void __send_error(at_error_code_t code, const char *message);

static void __handle_wifi_query();
static void __handle_wifitable_query();
static void __handle_wifi_set();
static bool __handle_bind_query(int &sent_code);
static void __send_bind_status();
static void __handle_saved_wifi_query();
static void __handle_unbind_query();
static void __process_command(const char *data);

namespace
{
    void atcmd_task()
    {
        if (!at_event_group)
        {
            return;
        }

        constexpr EventBits_t kEventMask = AT_EVENT_WIFI_CONNECT_BIT |
                                           AT_EVENT_WIFI_SCAN_BIT |
                                           AT_EVENT_WIFI_QUERY_BIT |
                                           AT_EVENT_CMD_RECEIVED_BIT |
                                           AT_EVENT_BIND_BIT;

        EventBits_t bits = xEventGroupWaitBits(at_event_group, kEventMask, pdTRUE, pdFALSE, 0);

        if (bits & AT_EVENT_WIFI_CONNECT_BIT)
        {
            __handle_wifi_set();
        }

        if (bits & AT_EVENT_WIFI_SCAN_BIT)
        {
            __handle_wifitable_query();
        }

        if (bits & AT_EVENT_WIFI_QUERY_BIT)
        {
            __handle_wifi_query();
        }

        if (bits & AT_EVENT_BIND_BIT)
        {
            __send_bind_status();
        }

        if ((bits & AT_EVENT_CMD_RECEIVED_BIT) && at_cmd_queue != nullptr)
        {
            char *received_data_ptr = nullptr;
            while (xQueueReceive(at_cmd_queue, &received_data_ptr, 0) == pdTRUE)
            {
                if (received_data_ptr != nullptr)
                {
                    __process_command(received_data_ptr);
                    free(received_data_ptr);
                }
            }
        }
    }

    bool g_atcmd_app_registered = false;
}

static void __send_response(const char *format, ...)
{
    if (!AppBLE::getInstance().isConnected())
    {
        Log.warningln("[app_atcmd] BLE not connected, cannot send response.");
        return;
    }

    char buffer[256 + 4];
    va_list args;
    va_start(args, format);
    int written_len = vsnprintf(buffer, sizeof(buffer) - 3, format, args);
    va_end(args);

    if (written_len > 0)
    {
        strcat(buffer, "\r\n");
    }

    Log.verboseln("[app_atcmd] Sending Response: %s", buffer);
    app_ble_send_message_external(buffer);
}

static void __send_error(at_error_code_t code, const char *message)
{
    __send_response("AT+ERROR:%d,%s", code, message);
}

static bool __send_json_with_ok(const char *json_string)
{
    if (!AppBLE::getInstance().isConnected())
    {
        Log.warningln("[app_atcmd] BLE not connected, cannot send response.");
        return false;
    }
    if (json_string == NULL)
    {
        Log.errorln("[app_atcmd] Cannot send a null JSON string.");
        return false;
    }

    size_t buffer_size = strlen(json_string) + 8;
    char *buffer = (char *)ps_malloc(buffer_size);
    if (buffer == NULL)
    {
        Log.errorln("[app_atcmd] Failed to allocate memory for response buffer.");
        return false;
    }

    strcpy(buffer, json_string);
    strcat(buffer, "\r\nok\r\n");

    Log.verboseln("[app_atcmd] Sending Response: %s", buffer);
    bool sent = app_ble_send_message_external(buffer);

    free(buffer);
    return sent;
}

static const char *__wifi_auth_to_string(wifi_auth_mode_t authmode)
{
    switch (authmode)
    {
    case WIFI_AUTH_OPEN:
        return "OPEN";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2_WPA3_PSK";
    default:
        return "UNKNOWN";
    }
}

static void __handle_wifi_query()
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        __send_error(AT_ERR_INTERNAL, "Memory allocation failed for root");
        return;
    }

    cJSON *data_obj = cJSON_CreateObject();
    if (data_obj == NULL)
    {
        cJSON_Delete(root);
        __send_error(AT_ERR_INTERNAL, "Memory allocation failed for data");
        return;
    }

    cJSON_AddStringToObject(root, "name", "wifi");
    cJSON_AddItemToObject(root, "data", data_obj);

    if (WiFi.isConnected())
    {
        cJSON_AddNumberToObject(root, "code", 1);

        cJSON_AddStringToObject(data_obj, "ssid", WiFi.SSID().c_str());

        char rssi_str[8];
        snprintf(rssi_str, sizeof(rssi_str), "%d", WiFi.RSSI());
        cJSON_AddStringToObject(data_obj, "rssi", rssi_str);

        cJSON_AddStringToObject(data_obj, "encryption", __wifi_auth_to_string(WiFi.encryptionType(0)));
    }
    else
    {
        cJSON_AddNumberToObject(root, "code", 0);
        cJSON_AddStringToObject(data_obj, "ssid", "");
        cJSON_AddStringToObject(data_obj, "rssi", "");
        cJSON_AddStringToObject(data_obj, "encryption", "");
    }

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL)
    {
        Log.verboseln("[app_atcmd] Wi-Fi query response payload: %s", json_string);
        __send_json_with_ok(json_string);
        free(json_string);
    }
    else
    {
        __send_error(AT_ERR_INTERNAL, "JSON serialization failed");
    }
    cJSON_Delete(root);
}

static void __handle_wifi_set()
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        Log.errorln("[app_atcmd] Failed to create JSON object for wifi response");
        return;
    }

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
    {
        Log.errorln("[app_atcmd] Failed to create data object for wifi response");
        cJSON_Delete(root);
        return;
    }

    cJSON_AddStringToObject(root, "name", "wifi");
    cJSON_AddNumberToObject(root, "code", g_wifi_connect_reason);
    cJSON_AddItemToObject(root, "data", data);
    cJSON_AddStringToObject(data, "ssid", g_wifi_connect_ssid);

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL)
    {
        Log.verboseln("[app_atcmd] Wi-Fi connect response payload: %s", json_string);
        __send_json_with_ok(json_string);
        free(json_string);
    }
    else
    {
        Log.errorln("[app_atcmd] Failed to print wifi response JSON");
    }

    cJSON_Delete(root);

    memset(g_wifi_connect_ssid, 0, sizeof(g_wifi_connect_ssid));
    g_wifi_connect_reason = 0;
}

static bool __handle_bind_query(int &sent_code)
{
    Log.verboseln("[app_atcmd] Handling bind query...");

    cJSON *response_json = cJSON_CreateObject();
    if (response_json == NULL)
    {
        __send_error(AT_ERR_INTERNAL, "Failed to create response JSON");
        return false;
    }

    cJSON_AddStringToObject(response_json, "name", "bind");

    String mac_addr = WiFi.macAddress();
    cJSON_AddStringToObject(response_json, "mac", mac_addr.c_str());

    sent_code = GetActivationCode();
    if (sent_code < 0)
    {
        cJSON_AddStringToObject(response_json, "state", "pending");
        cJSON_AddNumberToObject(response_json, "code", -1);
        Log.verboseln("[app_atcmd] Bind state is pending.");
    }
    else if (sent_code == 0)
    {
        cJSON_AddStringToObject(response_json, "state", "bound");
        cJSON_AddNumberToObject(response_json, "code", 0);
        Log.verboseln("[app_atcmd] Device is bound. Responding with code 0.");
    }
    else
    {
        cJSON_AddStringToObject(response_json, "state", "waiting");
        cJSON_AddNumberToObject(response_json, "code", sent_code);
        Log.verboseln("[app_atcmd] Device is not bound. Responding with unbound code.");
    }

    char *json_string = cJSON_PrintUnformatted(response_json);
    if (json_string != NULL)
    {
        Log.verboseln("[app_atcmd] Bind response payload: %s", json_string);
        bool sent = __send_json_with_ok(json_string);
        free(json_string);
        cJSON_Delete(response_json);
        return sent;
    }
    else
    {
        __send_error(AT_ERR_INTERNAL, "Failed to print response JSON");
    }

    cJSON_Delete(response_json);
    return false;
}

static void __send_bind_status()
{
    int sent_code = -1;
    bool sent = __handle_bind_query(sent_code);
    if (sent && sent_code == 0)
    {
        app_wifi_clear_origin();
        app_ble_stop_advertising();
    }
}

static void __handle_unbind_query()
{
    Log.verboseln("[app_atcmd] Handling unbind query...");
    ResetAfterUnbind();
}

static void __handle_saved_wifi_query()
{
    Log.verboseln("[app_atcmd] Handling saved Wi-Fi query...");

    std::vector<std::pair<String, String>> credentials;
    if (!GetWifiCredentials(credentials))
    {
        __send_error(AT_ERR_INTERNAL, "Failed to read saved Wi-Fi from NVS");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        __send_error(AT_ERR_INTERNAL, "JSON root creation failed");
        return;
    }

    cJSON *saved_array = cJSON_CreateArray();
    if (saved_array == NULL)
    {
        cJSON_Delete(root);
        __send_error(AT_ERR_INTERNAL, "JSON array creation failed");
        return;
    }
    cJSON_AddItemToObject(root, "saved_wifi", saved_array);

    for (const auto &cred : credentials)
    {
        cJSON *wifi_obj = cJSON_CreateObject();
        if (wifi_obj)
        {
            cJSON_AddStringToObject(wifi_obj, "ssid", cred.first.c_str());
            cJSON_AddStringToObject(wifi_obj, "password", cred.second.c_str());
            cJSON_AddItemToArray(saved_array, wifi_obj);
        }
    }

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL)
    {
        Log.verboseln("[app_atcmd] Saved Wi-Fi payload: %s", json_string);
        __send_json_with_ok(json_string);
        free(json_string);
    }
    else
    {
        __send_error(AT_ERR_INTERNAL, "JSON serialization failed");
    }

    cJSON_Delete(root);
}

static void __handle_wifitable_query()
{
    if (!AppBLE::getInstance().isConnected())
    {
        Log.verboseln("[app_atcmd] BLE not connected, cannot send Wi-Fi list.");
        return;
    }

    cJSON_Hooks psram_hooks;
    psram_hooks.malloc_fn = [](size_t sz)
    { return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM); };
    psram_hooks.free_fn = heap_caps_free;
    cJSON_InitHooks(&psram_hooks);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        Log.errorln("[app_atcmd] Failed to create cJSON root on PSRAM.");
        __send_error(AT_ERR_INTERNAL, "JSON root creation failed");
        cJSON_InitHooks(NULL);
        return;
    }

    cJSON *scanned_array = cJSON_CreateArray();

    if (scanned_array == NULL)
    {
        Log.errorln("[app_atcmd] Failed to create JSON arrays on PSRAM.");
        cJSON_Delete(root);
        __send_error(AT_ERR_INTERNAL, "JSON array creation failed");
        cJSON_InitHooks(NULL);
        return;
    }

    cJSON_AddItemToObject(root, "scanned_wifi", scanned_array);

    const auto &scan_cache = app_wifi_get_scan_cache();
    if (!scan_cache.empty())
    {
        for (const auto &net : scan_cache)
        {
            cJSON *network_obj = cJSON_CreateObject();
            if (network_obj == NULL)
            {
                continue;
            }
            cJSON_AddStringToObject(network_obj, "ssid", net.ssid);
            char rssi_str[8];
            snprintf(rssi_str, sizeof(rssi_str), "%d", net.rssi);
            cJSON_AddStringToObject(network_obj, "rssi", rssi_str);
            cJSON_AddStringToObject(network_obj, "encryption", __wifi_auth_to_string(net.authmode));
            cJSON_AddItemToArray(scanned_array, network_obj);
        }
    }

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL)
    {
        __send_json_with_ok(json_string);
        free(json_string);
    }
    else
    {
        __send_error(AT_ERR_INTERNAL, "JSON serialization failed");
    }

    cJSON_Delete(root);
    cJSON_InitHooks(NULL);
}

static void __ctrl_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    switch (id)
    {
    case APP_ATCMD_WIFI_SCAN:
    {
        xEventGroupSetBits(at_event_group, AT_EVENT_WIFI_SCAN_BIT);
        break;
    }

    case APP_ATCMD_WIFI_GET:
    {

        break;
    }

    case APP_ATCMD_WIFI_CONNECT:
    {
        wifi_connect_result_t *result = (wifi_connect_result_t *)event_data;
        strncpy(g_wifi_connect_ssid, result->ssid, sizeof(g_wifi_connect_ssid) - 1);
        g_wifi_connect_reason = result->reason_code;
        xEventGroupSetBits(at_event_group, AT_EVENT_WIFI_CONNECT_BIT);
        break;
    }

    case APP_ATCMD_BIND:
    {
        xEventGroupSetBits(at_event_group, AT_EVENT_BIND_BIT);
        break;
    }

    case APP_ATCMD_BLE_RX:
    {
        app_power_manager_activity(AppPowerOwner::Config);
        char *rx_data_ptr = (char *)event_data;

        char *data_copy = (char *)ps_malloc(strlen(rx_data_ptr) + 1);
        if (data_copy == NULL)
        {
            Log.errorln("[app_atcmd] Failed to malloc for AT command copy!");
            return;
        }
        strcpy(data_copy, rx_data_ptr);

        if (xQueueSend(at_cmd_queue, &data_copy, pdMS_TO_TICKS(10)) != pdTRUE)
        {
            Log.warningln("[app_atcmd] AT command queue is full. Discarding command.");
            free(data_copy);
            return;
        }

        xEventGroupSetBits(at_event_group, AT_EVENT_CMD_RECEIVED_BIT);
        break;
    }

    default:
        break;
    }
}

static void __process_command(const char *data)
{
    Log.verboseln("[app_atcmd] Processing command in task: '%s'", data);

    if (strstr(data, "AT+wifitable?") != NULL)
    {
        Log.verboseln("[app_atcmd] Handling command: AT+wifitable?");
        esp_event_post(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_SCAN, NULL, 0, portMAX_DELAY);
    }
    else if (strstr(data, "AT+savedwifi?") != NULL)
    {
        Log.verboseln("[app_atcmd] Handling command: AT+savedwifi?");
        __handle_saved_wifi_query();
    }
    else if (strstr(data, "AT+wifi?") != NULL)
    {
        Log.verboseln("[app_atcmd] Handling command: AT+wifi?");
        xEventGroupSetBits(at_event_group, AT_EVENT_WIFI_QUERY_BIT);
    }
    else if (strncmp(data, "AT+wifi=", 8) == 0)
    {
        Log.verboseln("[app_atcmd] Handling command: AT+wifi=");
        const char *json_payload = data + 8;
        Log.verboseln("[app_atcmd] Extracted JSON: %s", json_payload);

        cJSON *root = cJSON_Parse(json_payload);
        if (root != NULL)
        {
            cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
            cJSON *password_item = cJSON_GetObjectItem(root, "password");

            if (cJSON_IsString(ssid_item))
            {
                const char *ssid = ssid_item->valuestring;
                const char *password = cJSON_IsString(password_item) ? password_item->valuestring : "";

                Log.verboseln("[app_atcmd] Requesting Wi-Fi connection. SSID: %s", ssid);

                app_wifi_connect_args_t args;
                memset(&args, 0, sizeof(args));
                strlcpy(args.ssid, ssid, sizeof(args.ssid));
                strlcpy(args.password, password, sizeof(args.password));
                args.origin = ProvisioningOrigin::BleAt;
                esp_event_post(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_CONNECT, &args, sizeof(args), portMAX_DELAY);
            }
            else
            {
                Log.errorln("[app_atcmd] JSON parsing error: 'ssid' field is missing or not a string.");
            }
            cJSON_Delete(root);
        }
        else
        {
            Log.errorln("[app_atcmd] Failed to parse JSON payload.");
        }
    }
    else if (strstr(data, "AT+bind?") != NULL)
    {
        Log.verboseln("[app_atcmd] Handling command: AT+bind?");
        __send_bind_status();
    }
    else if (strstr(data, "AT+unbind") != NULL)
    {
        Log.verboseln("[app_atcmd] Handling command: AT+unbind");
        __handle_unbind_query();
    }
    else
    {
        Log.warningln("[app_atcmd] Received unknown command or malformed data: %s", data);
    }
}

void app_atcmd_init()
{
    if (g_atcmd_app_registered)
    {
        Log.warningln("[app_atcmd] AT command app already running.");
        return;
    }

    if (!at_event_group)
    {
        at_event_group = xEventGroupCreate();
        if (at_event_group == NULL)
        {
            Log.errorln("Error: Failed to create AT event group.");
            return;
        }
    }

    if (!at_cmd_queue)
    {
        at_cmd_queue = xQueueCreate(10, sizeof(char *));
        if (at_cmd_queue == NULL)
        {
            Log.errorln("[app_atcmd] Failed to create AT command queue.");
            return;
        }
    }

    ESP_ERROR_CHECK(esp_event_handler_register(CTRL_EVENT_BASE, APP_ATCMD_WIFI_SCAN, &__ctrl_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(CTRL_EVENT_BASE, APP_ATCMD_WIFI_GET, &__ctrl_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(CTRL_EVENT_BASE, APP_ATCMD_WIFI_CONNECT, &__ctrl_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(CTRL_EVENT_BASE, APP_ATCMD_BLE_RX, &__ctrl_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(CTRL_EVENT_BASE, APP_ATCMD_BIND, &__ctrl_event_handler, NULL));

    if (!MiniRT::addTask(atcmd_task, 10, MiniRT::Priority::Low))
    {
        Log.errorln("[app_atcmd] Failed to register AT command task with MiniRT");
        return;
    }

    g_atcmd_app_registered = true;
}
