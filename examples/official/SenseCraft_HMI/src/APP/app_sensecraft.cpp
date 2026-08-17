#include "APP/app_sensecraft.h"

#include "APP/app_device_info.h"
#include "APP/app_download.h"
#include "APP/app_events.h"
#include "APP/app_gallery.h"
#include "APP/app_power_manager.h"
#include "APP/app_user_action.h"
#include "APP/app_view.h"
#include "APP/app_wifi.h"
#include "APP/iot.h"

#include "app_config.h"
#include "hal/hal.h"
#include "hal/hal_indicator.h"

#include "ArduinoLog.h"
#include "cJSON.h"
#include "mqtt_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>

#include <cmath>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <sys/time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "mbedtls/platform.h"

ESP_EVENT_DEFINE_BASE(CTRL_EVENT_BASE);

namespace
{
enum class SenseCraftState
{
    Offline,
    NeedSession,
    WaitingActivation,
    MqttConnecting,
    Online,
};

constexpr EventBits_t EVT_ENSURE_SESSION = 1 << 0;
constexpr EventBits_t EVT_BIND_RETRY = 1 << 1;
constexpr EventBits_t EVT_BIND_POLL = 1 << 2;
constexpr EventBits_t EVT_MQTT_CONNECT = 1 << 3;
constexpr EventBits_t EVT_SCREEN_REFRESH = 1 << 4;
constexpr EventBits_t EVT_IOT_REPORT = 1 << 5;
constexpr EventBits_t EVT_SLEEP_PUBLISHED = 1 << 6;

constexpr uint64_t BIND_RETRY_BACKOFF_US = 60ULL * 1000ULL * 1000ULL;
constexpr uint64_t BIND_POLL_US = 10ULL * 1000ULL * 1000ULL;
constexpr uint64_t FIRST_REPORT_DELAY_US = 2ULL * 1000ULL * 1000ULL;
constexpr uint64_t IMAGE_REFRESH_RETRY_US = 5ULL * 1000ULL * 1000ULL;
constexpr uint64_t IOT_REPORT_INTERVAL_US = 60ULL * 1000ULL * 1000ULL;
constexpr uint64_t CONFIG_REPORT_DEBOUNCE_US = 500ULL * 1000ULL;
constexpr uint32_t BIND_WIFI_WAIT_TIMEOUT_MS = 2000;
constexpr uint32_t BIND_WIFI_WAIT_STEP_MS = 100;
constexpr uint32_t SENSECRAFT_TASK_STACK_SIZE = 10240;
constexpr UBaseType_t SENSECRAFT_TASK_PRIORITY = tskIDLE_PRIORITY + 1;
constexpr int IMAGE_REFRESH_MAX_RETRY = 3;

SenseCraftState g_state = SenseCraftState::Offline;
EventGroupHandle_t g_events = nullptr;
TaskHandle_t g_task = nullptr;
SemaphoreHandle_t g_sleep_mutex = nullptr;

esp_mqtt_client_handle_t g_mqtt = nullptr;
MqttConfig g_mqtt_cfg;
BoardInfo g_board;
String g_last_image_session;
String g_mqtt_payload;
String g_refresh_target_id;

esp_timer_handle_t g_bind_retry_timer = nullptr;
esp_timer_handle_t g_bind_poll_timer = nullptr;
esp_timer_handle_t g_first_report_timer = nullptr;
esp_timer_handle_t g_image_retry_timer = nullptr;
esp_timer_handle_t g_iot_timer = nullptr;
esp_timer_handle_t g_refresh_timer = nullptr;
esp_timer_handle_t g_config_report_timer = nullptr;

bool g_started = false;
bool g_registered = false;
bool g_network_ready = false;
bool g_image_res_received = true;
bool g_first_image_res_received = false;
bool g_waiting_for_content = false;
bool g_cloud_wait_power_held = false;
int g_bind_retry_count = 0;
int g_image_retry_count = 0;
int g_last_activation_code = -1;
int g_sleep_msg_id = -1;
std::atomic<bool> g_config_report_pending{false};
std::atomic<uint32_t> g_config_report_generation{0};
std::atomic<int> g_config_report_msg_id{-1};
std::atomic<uint32_t> g_config_report_msg_generation{0};

void mqttEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void ctrlEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data);
void sensecraftTask(void *param);
void configReportCb(void *arg);
void stopTimer(esp_timer_handle_t timer);

void scheduleConfigReport()
{
    if (!g_config_report_timer)
    {
        return;
    }

    stopTimer(g_config_report_timer);
    esp_err_t err = esp_timer_start_once(g_config_report_timer, CONFIG_REPORT_DEBOUNCE_US);
    if (err != ESP_OK)
    {
        Log.warningln(F("[app_sensecraft] Config report timer start failed: %s"), esp_err_to_name(err));
    }
}

String takeRefreshTargetId()
{
    String id = g_refresh_target_id;
    g_refresh_target_id = "";
    return id;
}

void setState(SenseCraftState state)
{
    g_state = state;
}

void holdCloudWaitPower()
{
    if (g_cloud_wait_power_held)
    {
        return;
    }

    app_power_manager_acquire(AppPowerOwner::CloudWait);
    g_cloud_wait_power_held = true;
}

void releaseCloudWaitPower()
{
    if (!g_cloud_wait_power_held)
    {
        return;
    }

    app_power_manager_release(AppPowerOwner::CloudWait);
    g_cloud_wait_power_held = false;
}

uint64_t timestampMs()
{
    uint64_t now_ms = static_cast<uint64_t>(time(nullptr)) * 1000ULL;
    if (now_ms == 0 && millis() > 10000)
    {
        now_ms = millis();
    }
    return now_ms;
}

String sessionId()
{
    char buffer[11];
    snprintf(buffer, sizeof(buffer), "%u", esp_random());
    return String(buffer);
}

esp_err_t createTimer(const char *name, esp_timer_cb_t cb, esp_timer_handle_t *timer)
{
    esp_timer_create_args_t args = {};
    args.callback = cb;
    args.name = name;
    return esp_timer_create(&args, timer);
}

void stopTimer(esp_timer_handle_t timer)
{
    if (timer && esp_timer_is_active(timer))
    {
        esp_timer_stop(timer);
    }
}

void syncServerTime(cJSON *server_time)
{
    if (!cJSON_IsObject(server_time))
    {
        Log.warningln(F("[app_sensecraft] Server time missing, skip time sync."));
        return;
    }

    cJSON *timestamp_item = cJSON_GetObjectItem(server_time, "timestamp");
    if (!cJSON_IsNumber(timestamp_item) || timestamp_item->valuedouble <= 0)
    {
        Log.warningln(F("[app_sensecraft] Invalid server timestamp."));
        return;
    }

    int offset_hours = 0;
    cJSON *offset_item = cJSON_GetObjectItem(server_time, "timezone_offset");
    if (cJSON_IsNumber(offset_item))
    {
        offset_hours = offset_item->valueint / 60;
    }

    char tz[32];
    snprintf(tz, sizeof(tz), "LCL%d", -offset_hours);
    setenv("TZ", tz, 1);
    tzset();

    long long timestamp_ms = static_cast<long long>(timestamp_item->valuedouble);
    timeval tv = {};
    tv.tv_sec = timestamp_ms / 1000;
    tv.tv_usec = (timestamp_ms % 1000) * 1000;
    if (settimeofday(&tv, nullptr) != 0)
    {
        Log.errorln(F("[app_sensecraft] Failed to set system time."));
        return;
    }

    time_t now = time(nullptr);
    tm local_time = {};
    localtime_r(&now, &local_time);

    if (HAL::GetHAL().rtcSetTime(local_time.tm_year + 1900,
                                 local_time.tm_mon + 1,
                                 local_time.tm_mday,
                                 local_time.tm_hour,
                                 local_time.tm_min,
                                 local_time.tm_sec))
    {
        Log.infoln(F("[app_sensecraft] Time synced from server."));
    }
    else
    {
        Log.errorln(F("[app_sensecraft] RTC update failed after server time sync."));
    }
}

bool buildBindBody(String &body)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return false;
    }

    cJSON_AddStringToObject(root, "mac_address", g_board.boardMac.c_str());
    cJSON_AddStringToObject(root, "chip_model_name", chip_model_name);
    cJSON_AddStringToObject(root, "version", g_currentAppVersion);

    cJSON *board = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "board", board);
    cJSON_AddStringToObject(board, "type", g_board.type.c_str());
    cJSON_AddStringToObject(board, "screen_type", g_board.screen_type.c_str());
    cJSON_AddStringToObject(board, "resolution", g_board.resolution.c_str());
    cJSON_AddStringToObject(board, "ssid", WiFi.SSID().c_str());
    cJSON_AddNumberToObject(board, "rssi", WiFi.RSSI());
    cJSON_AddNumberToObject(board, "channel", WiFi.channel());
    cJSON_AddStringToObject(board, "ip", WiFi.localIP().toString().c_str());
    cJSON_AddStringToObject(board, "mac", g_board.boardMac.c_str());
    cJSON_AddStringToObject(board, "img_format", g_board.img_type.c_str());

    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        body = json;
        Log.verboseln(F("[app_sensecraft] Bind body: %s"), body.c_str());
        cJSON_free(json);
    }

    cJSON_Delete(root);
    return json != nullptr;
}

bool postJson(const String &url, const String &body, String &resp, int &http_code)
{
    std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure());
    client->setInsecure();
    client->setHandshakeTimeout((HTTP_REQUEST_TIMEOUT_MS + 999) / 1000);

    HTTPClient http;
    if (!http.begin(*client, url))
    {
        http_code = -1;
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_REQUEST_TIMEOUT_MS);

    http_code = http.POST(body);
    if (http_code == HTTP_CODE_OK)
    {
        resp = http.getString();
    }
    http.end();

    return http_code == HTTP_CODE_OK;
}

void parseMqttEndpoint(const char *endpoint, MqttConfig &cfg)
{
    String value = endpoint;
    int sep = value.indexOf(':');
    if (sep > 0)
    {
        cfg.host = value.substring(0, sep);
        cfg.port = value.substring(sep + 1).toInt();
    }
    else
    {
        cfg.host = value;
        cfg.port = 1883;
    }
}

bool parseBindResp(const String &resp, MqttConfig &cfg, String &token, int &activation_code, String &activation_msg)
{
    cJSON *root = cJSON_Parse(resp.c_str());
    if (!root)
    {
        return false;
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsNumber(code) || code->valueint != 200 || !cJSON_IsObject(result))
    {
        cJSON_Delete(root);
        return false;
    }

    syncServerTime(cJSON_GetObjectItem(result, "server_time"));

    cJSON *mqtt = cJSON_GetObjectItem(result, "mqtt");
    if (!cJSON_IsObject(mqtt))
    {
        cJSON_Delete(root);
        return false;
    }

    cJSON *endpoint = cJSON_GetObjectItem(mqtt, "endpoint");
    if (cJSON_IsString(endpoint))
    {
        parseMqttEndpoint(endpoint->valuestring, cfg);
    }

    cJSON *client_id = cJSON_GetObjectItem(mqtt, "client_id");
    if (cJSON_IsString(client_id))
    {
        cfg.clientID = client_id->valuestring;
    }

    cJSON *username = cJSON_GetObjectItem(mqtt, "username");
    if (cJSON_IsString(username))
    {
        cfg.username = username->valuestring;
    }

    cJSON *password = cJSON_GetObjectItem(mqtt, "password");
    if (cJSON_IsString(password))
    {
        cfg.password = password->valuestring;
        token = password->valuestring;
        // Log.infoln(F("[app_sensecraft] MQTT password: %s"), cfg.password.c_str());
    }

    cJSON *publish_topic = cJSON_GetObjectItem(mqtt, "publish_topic");
    if (cJSON_IsString(publish_topic))
    {
        cfg.publishTopic = publish_topic->valuestring;
    }

    cJSON *subscribe_topic = cJSON_GetObjectItem(mqtt, "subscribe_topic");
    if (cJSON_IsString(subscribe_topic))
    {
        cfg.subscribeTopic = subscribe_topic->valuestring;
    }

    activation_code = -1;
    activation_msg.clear();
    cJSON *activation = cJSON_GetObjectItem(result, "activation");
    if (cJSON_IsObject(activation))
    {
        cJSON *act_code = cJSON_GetObjectItem(activation, "code");
        cJSON *act_msg = cJSON_GetObjectItem(activation, "message");
        if (cJSON_IsNumber(act_code))
        {
            activation_code = act_code->valueint;
        }
        if (cJSON_IsString(act_msg))
        {
            activation_msg = act_msg->valuestring;
        }
    }

    cJSON_Delete(root);
    return true;
}

bool requestBind(MqttConfig &cfg, String &token, int &activation_code, String &activation_msg, int &http_code)
{
    unsigned long started_at = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - started_at >= BIND_WIFI_WAIT_TIMEOUT_MS)
        {
            http_code = -1;
            return false;
        }
        delay(BIND_WIFI_WAIT_STEP_MS);
    }

    String body;
    if (!buildBindBody(body))
    {
        http_code = -1;
        return false;
    }

    String resp;
    String url = String(API_BASE_URL) + API_PATH_DEVICE_BIND;
    if (!postJson(url, body, resp, http_code))
    {
        return false;
    }

    return parseBindResp(resp, cfg, token, activation_code, activation_msg);
}

void stopMqtt(bool destroy)
{
    stopTimer(g_first_report_timer);
    stopTimer(g_iot_timer);

    if (!g_mqtt)
    {
        return;
    }

    esp_mqtt_client_disconnect(g_mqtt);
    if (destroy)
    {
        esp_mqtt_client_stop(g_mqtt);
        esp_mqtt_client_destroy(g_mqtt);
        g_mqtt = nullptr;
    }
}

void resetSession()
{
    stopMqtt(true);
    releaseCloudWaitPower();
    g_mqtt_cfg = MqttConfig();
    g_image_retry_count = 0;
    g_image_res_received = true;
    g_first_image_res_received = false;
    g_waiting_for_content = false;
    setState(g_network_ready ? SenseCraftState::NeedSession : SenseCraftState::Offline);
}

int publish(cJSON *doc)
{
    if (g_state != SenseCraftState::Online || !g_mqtt || g_mqtt_cfg.publishTopic.isEmpty())
    {
        Log.warningln(F("[app_sensecraft] MQTT publish skipped, session is not online."));
        return -1;
    }

    char *payload = cJSON_PrintUnformatted(doc);
    if (!payload)
    {
        return -1;
    }

#if (RETERMINAL_INFO_DEBUG)
    Log.infoln(F("[app_sensecraft] Publish [%s]: %s"), g_mqtt_cfg.publishTopic.c_str(), payload);
#else
    Log.verboseln(F("[app_sensecraft] Publish [%s]: %s"), g_mqtt_cfg.publishTopic.c_str(), payload);
#endif

    int msg_id = esp_mqtt_client_publish(g_mqtt, g_mqtt_cfg.publishTopic.c_str(), payload, strlen(payload), 1, 1);
    cJSON_free(payload);
    return msg_id;
}

void connectMqtt()
{
    if (!g_network_ready || g_mqtt_cfg.host.isEmpty())
    {
        setState(g_network_ready ? SenseCraftState::NeedSession : SenseCraftState::Offline);
        xEventGroupSetBits(g_events, EVT_ENSURE_SESSION);
        return;
    }

    stopMqtt(true);
    setState(SenseCraftState::MqttConnecting);

    Log.verboseln(F("[app_sensecraft] MQTT connect %s:%d"), g_mqtt_cfg.host.c_str(), g_mqtt_cfg.port);
    // Log.infoln(F("[app_sensecraft] MQTT password: %s"), g_mqtt_cfg.password.c_str());

    esp_mqtt_client_config_t cfg = {
        .host = g_mqtt_cfg.host.c_str(),
        .port = static_cast<uint32_t>(g_mqtt_cfg.port),
        .client_id = g_mqtt_cfg.clientID.c_str(),
        .username = g_mqtt_cfg.username.c_str(),
        .password = g_mqtt_cfg.password.c_str(),
        .disable_clean_session = true,
        .keepalive = 60,
        .disable_auto_reconnect = true,
#if !HMI_TEST_ENV
        .cert_pem = reinterpret_cast<const char *>(rootCACertificate),
        .transport = MQTT_TRANSPORT_OVER_SSL,
#else
        .cert_pem = nullptr,
#endif
        .reconnect_timeout_ms = MQTT_RECONNECT_INTERVAL_MS,
    };

    g_mqtt = esp_mqtt_client_init(&cfg);
    if (!g_mqtt)
    {
        Log.errorln(F("[app_sensecraft] MQTT init failed."));
        resetSession();
        xEventGroupSetBits(g_events, EVT_ENSURE_SESSION);
        return;
    }

    esp_mqtt_client_register_event(g_mqtt, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqttEventHandler, nullptr);
    esp_err_t err = esp_mqtt_client_start(g_mqtt);
    if (err != ESP_OK)
    {
        Log.errorln(F("[app_sensecraft] MQTT start failed: %s"), esp_err_to_name(err));
        if (!IsTimerWakeup())
        {
            hal_indicator_play(HAL_INDICATOR_ERROR);
        }
        resetSession();
        xEventGroupSetBits(g_events, EVT_ENSURE_SESSION);
    }
}

void postActivation(int code, const String &msg)
{
    const bool new_code = g_last_activation_code != code;
    g_last_activation_code = code;

    SetActivationCode(code);
    SetBindState(false);

    if (app_wifi_get_origin() == ProvisioningOrigin::BleAt)
    {
        if (new_code)
        {
            Log.infoln(F("[app_sensecraft] Activation code ready for BLE, code=%d."), code);
            esp_event_post(CTRL_EVENT_BASE, APP_ATCMD_BIND, nullptr, 0, portMAX_DELAY);
        }
    }
    else if (new_code || !app_view_is_showing_activation())
    {
        ActivationEventData evt = {};
        evt.activationCode = code;
        strncpy(evt.activationMsg, msg.c_str(), sizeof(evt.activationMsg) - 1);

        esp_event_post(VIEW_EVENT_BASE,
                       VIEW_EVENT_SHOW_ACTIVATION_CODE,
                       &evt,
                       sizeof(evt),
                       portMAX_DELAY);
    }

    stopTimer(g_bind_poll_timer);
    esp_timer_start_once(g_bind_poll_timer, BIND_POLL_US);
}

void ensureSession()
{
    if (!g_network_ready)
    {
        setState(SenseCraftState::Offline);
        return;
    }

    const auto &screen = HAL::GetHAL().screen();
    g_board.type = screen.board_info_type;
    g_board.screen_type = screen.board_info_screen_type;
    g_board.ssid = WiFi.SSID();
    g_board.rssi = WiFi.RSSI();
    g_board.channel = WiFi.channel();
    g_board.ip = WiFi.localIP().toString();
    g_board.boardMac = WiFi.macAddress();
    g_board.resolution = screen.resolution;
    g_board.img_type = "epd";

    Log.infoln(F("[app_sensecraft] Bind session, mac=%s, ip=%s, app=%s"),
               g_board.boardMac.c_str(),
               g_board.ip.c_str(),
               g_currentAppVersion);

    MqttConfig cfg;
    String token;
    String activation_msg;
    int activation_code = -1;
    int http_code = -1;

    if (!requestBind(cfg, token, activation_code, activation_msg, http_code))
    {
        g_bind_retry_count++;
        if (g_bind_retry_count < MAX_RETRIES)
        {
            Log.errorln(F("[app_sensecraft] Bind failed, retry=%d/%d."), g_bind_retry_count, MAX_RETRIES);
            esp_timer_start_once(g_bind_retry_timer, INITIAL_BIND_API_RETRY_DELAY_MS * 1000ULL);
        }
        else if (GetDeepSleepEnabled())
        {
            Log.errorln(F("[app_sensecraft] Bind failed after retries, enter deep sleep. http=%d"), http_code);
            EnterDeepSleep();
        }
        else
        {
            Log.warningln(F("[app_sensecraft] Bind failed after retries, retry later. http=%d"), http_code);
            g_bind_retry_count = 0;
            esp_timer_start_once(g_bind_retry_timer, BIND_RETRY_BACKOFF_US);
        }
        return;
    }

    g_bind_retry_count = 0;

    if (activation_code != 0)
    {
        Log.infoln(F("[app_sensecraft] Waiting activation, code=%d."), activation_code);
        if (GetBindState())
        {
            Log.warningln(F("[app_sensecraft] Server reports device unbound, resetting local device state."));
            releaseCloudWaitPower();
            ResetAfterUnbind();
            return;
        }
        setState(SenseCraftState::WaitingActivation);
        holdCloudWaitPower();
        postActivation(activation_code, activation_msg);
        return;
    }

    Log.infoln(F("[app_sensecraft] Session ready."));
    bool was_bound = GetBindState();
    ProvisioningOrigin origin = app_wifi_get_origin();
    stopTimer(g_bind_poll_timer);
    releaseCloudWaitPower();
    g_mqtt_cfg = cfg;
    g_last_activation_code = -1;
    SetCloudToken(token);
    SetActivationCode(0);
    SetBindState(true);

    if (!was_bound)
    {
        esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_WAITING, nullptr, 0, portMAX_DELAY);
    }

    if (origin == ProvisioningOrigin::BleAt)
    {
        esp_event_post(CTRL_EVENT_BASE, APP_ATCMD_BIND, nullptr, 0, portMAX_DELAY);
    }
    else
    {
        app_wifi_clear_origin();
    }
    xEventGroupSetBits(g_events, EVT_MQTT_CONNECT);
}

bool sendImageRefreshReq()
{
    if (g_state != SenseCraftState::Online)
    {
        return false;
    }

    cJSON *doc = cJSON_CreateObject();
    if (!doc)
    {
        return false;
    }

    String version = GetContentVersion();
    cJSON_AddStringToObject(doc, "version", Protocol_version);
    cJSON_AddStringToObject(doc, "session_id", sessionId().c_str());
    cJSON_AddStringToObject(doc, "type", "img_flash");
    cJSON_AddNumberToObject(doc, "timestamp", static_cast<double>(timestampMs()));

    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(doc, "data", data);
    cJSON_AddStringToObject(data, "version", version.c_str());

    Log.verboseln(F("[app_sensecraft] Request image refresh, version=%s."), version.c_str());
    bool ok = publish(doc) >= 0;
    cJSON_Delete(doc);
    return ok;
}

void handleImageRefreshReq()
{
    if (g_image_retry_count >= IMAGE_REFRESH_MAX_RETRY)
    {
        stopTimer(g_image_retry_timer);
        g_image_retry_count = 0;
        g_image_res_received = true;

        if (GetDeepSleepEnabled())
        {
            Log.warningln(F("[app_sensecraft] Image refresh timeout, enter deep sleep."));
            EnterDeepSleep();
        }
        else
        {
            Log.warningln(F("[app_sensecraft] Image refresh timeout, wait next timer."));
            request_update_timer_start();
        }
        return;
    }

    g_image_res_received = false;
    bool sent = sendImageRefreshReq();
    g_image_retry_count++;
    stopTimer(g_image_retry_timer);
    esp_timer_start_once(g_image_retry_timer, IMAGE_REFRESH_RETRY_US);

    if (!sent)
    {
        Log.warningln(F("[app_sensecraft] Image refresh request failed, retry=%d/%d."),
                      g_image_retry_count,
                      IMAGE_REFRESH_MAX_RETRY);
    }
}

void handleCloudCommand(cJSON *doc)
{
    cJSON *data = cJSON_GetObjectItem(doc, "data");
    cJSON *commands = cJSON_IsObject(data) ? cJSON_GetObjectItem(data, "commands") : nullptr;
    if (!cJSON_IsArray(commands))
    {
        Log.warningln(F("[app_sensecraft] IOT command missing commands array."));
        return;
    }

    cJSON *cmd = nullptr;
    cJSON_ArrayForEach(cmd, commands)
    {
        cJSON *name_item = cJSON_GetObjectItem(cmd, "name");
        cJSON *method_item = cJSON_GetObjectItem(cmd, "method");
        cJSON *params = cJSON_GetObjectItem(cmd, "parameters");
        if (!cJSON_IsString(name_item) || !cJSON_IsString(method_item) || !cJSON_IsObject(params))
        {
            Log.warningln(F("[app_sensecraft] Skip invalid IOT command."));
            continue;
        }

        const char *name = name_item->valuestring;
        const char *method = method_item->valuestring;

        if (strcmp(name, "DataAccess") == 0 && strcmp(method, "SetInterval") == 0)
        {
            cJSON *interval_item = cJSON_GetObjectItem(params, "interval");
            uint32_t interval = cJSON_IsNumber(interval_item) ? interval_item->valueint : 0;
            Log.infoln(F("[app_sensecraft] DataAccess.SetInterval=%u"), interval);
            SetDeepSleepInterval(interval);
            stopTimer(g_refresh_timer);
            request_update_timer_start();
        }
        else if (strcmp(name, "Power") == 0 && strcmp(method, "SetDeepSleep") == 0)
        {
            // enable=0 means deep sleep allowed, enable=1 means deep sleep disabled.
            cJSON *enable_item = cJSON_GetObjectItem(params, "enable");
            bool enabled = cJSON_IsNumber(enable_item) && enable_item->valueint == 0;
            Log.infoln(F("[app_sensecraft] Power.SetDeepSleep=%d (deep sleep %s)"),
                       cJSON_IsNumber(enable_item) ? enable_item->valueint : -1,
                       enabled ? "enabled" : "disabled");
            SetDeepSleepEnabled(enabled);
            if (enabled)
            {
                StartDeepSleepTimer();
            }
            else
            {
                StopDeepSleepTimer();
            }
        }
        else
        {
            Log.warningln(F("[app_sensecraft] Unknown IOT command: %s.%s"), name, method);
        }
    }
}

void handleImageResource(cJSON *doc)
{
    g_image_res_received = true;
    g_image_retry_count = 0;
    g_first_image_res_received = true;
    stopTimer(g_image_retry_timer);

    String target_id = takeRefreshTargetId();
    cJSON *data = cJSON_GetObjectItem(doc, "data");
    if (!cJSON_IsObject(data))
    {
        Log.errorln(F("[app_sensecraft] Image resource missing data."));
        return;
    }

    cJSON *session_item = cJSON_GetObjectItem(data, "session_id");
    if (cJSON_IsString(session_item))
    {
        const char *sid = session_item->valuestring;
        if (g_last_image_session.equals(sid))
        {
            Log.verboseln(F("[app_sensecraft] Duplicate image session: %s"), sid);
            return;
        }
        g_last_image_session = sid;
    }

    cJSON *version_item = cJSON_GetObjectItem(data, "version");
    cJSON *manifest_item = cJSON_GetObjectItem(data, "manifest_url");
    if (!cJSON_IsString(version_item) || !cJSON_IsString(manifest_item))
    {
        Log.errorln(F("[app_sensecraft] Image resource missing version or manifest_url."));
        return;
    }

    const char *version = version_item->valuestring;
    const char *manifest_url = manifest_item->valuestring;
    if (!manifest_url || manifest_url[0] == '\0')
    {
        if (g_waiting_for_content)
        {
            Log.verboseln(F("[app_sensecraft] Empty image manifest repeated."));
            return;
        }

        Log.infoln(F("[app_sensecraft] Empty image manifest, showing waiting page. version=%s."), version);
        g_waiting_for_content = true;
        holdCloudWaitPower();
        stopTimer(g_image_retry_timer);
        esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_WAITING, nullptr, 0, portMAX_DELAY);
        request_update_timer_start();
        return;
    }

    g_waiting_for_content = false;
    releaseCloudWaitPower();
    if (target_id.isEmpty() && app_download_is_same_manifest_in_progress(version, manifest_url))
    {
        Log.warningln(F("[app_sensecraft] Image manifest already in progress, version=%s."), version);
        return;
    }

    if (!target_id.isEmpty())
    {
        Log.infoln(F("[app_sensecraft] Image manifest received, version=%s, target=%s."),
                   version,
                   target_id.c_str());
    }
    else
    {
        Log.infoln(F("[app_sensecraft] Image manifest received, version=%s."), version);
    }

    app_download_set_manifest_url(manifest_url, version, target_id.isEmpty() ? nullptr : target_id.c_str());
    request_update_timer_start();
}

void handleMqttData(esp_mqtt_event_handle_t event)
{
    const bool partial = event->total_data_len > event->data_len || event->current_data_offset > 0;
    String payload;
    if (partial)
    {
        if (event->current_data_offset == 0)
        {
            g_mqtt_payload = "";
            g_mqtt_payload.reserve(event->total_data_len + 1);
        }

        g_mqtt_payload.concat(event->data, event->data_len);
        if (event->current_data_offset + event->data_len < event->total_data_len)
        {
            return;
        }

        payload = g_mqtt_payload;
        g_mqtt_payload = "";
    }
    else
    {
        payload.concat(event->data, event->data_len);
    }

    Log.verboseln(F("[app_sensecraft] MQTT data [%.*s]: %s"), event->topic_len, event->topic, payload.c_str());

    cJSON *doc = cJSON_Parse(payload.c_str());
    if (!doc)
    {
        Log.errorln(F("[app_sensecraft] Invalid MQTT JSON."));
        return;
    }

    cJSON *type_item = cJSON_GetObjectItem(doc, "type");
    if (!cJSON_IsString(type_item))
    {
        Log.warningln(F("[app_sensecraft] MQTT message missing type."));
        cJSON_Delete(doc);
        return;
    }

    const char *type = type_item->valuestring;
    if (strcmp(type, "iot") == 0)
    {
        app_power_manager_activity(AppPowerOwner::Config);
        handleCloudCommand(doc);
    }
    else if (strcmp(type, "img_flash") == 0 || strcmp(type, "album") == 0)
    {
        app_power_manager_activity(AppPowerOwner::Download);
        handleImageResource(doc);
    }
    else
    {
        Log.warningln(F("[app_sensecraft] Unknown MQTT message type: %s"), type);
    }

    cJSON_Delete(doc);
}

int publishIotReport(int status = DEVICE_ONLINE)
{
    if (g_state != SenseCraftState::Online)
    {
        return -1;
    }

    auto pub = [](cJSON *doc) -> int {
        return publish(doc);
    };

    IotReportBuilder builder(sessionId(), pub);
    builder
        .addState("Buttons", [](cJSON *st) {
            cJSON_AddNumberToObject(st, "left", HAL::GetHAL().buttonIsPressed(1) ? 0 : 1);
            cJSON_AddNumberToObject(st, "right", HAL::GetHAL().buttonIsPressed(2) ? 0 : 1);
        })
        .addState("DataAccess", [](cJSON *st) {
            cJSON_AddNumberToObject(st, "interval", GetDeepSleepInterval());
        })
        .addState("Battery", [](cJSON *st) {
            cJSON_AddNumberToObject(st, "level", static_cast<int>(HAL::GetHAL().batteryReadPercent() + 0.5f));
            cJSON_AddBoolToObject(st, "charging", HAL::GetHAL().pmicIsCharging());
        })
        .addState("Power", [](cJSON *st) {
            cJSON_AddNumberToObject(st, "deep_sleep_disabled", GetDeepSleepEnabled() ? 0 : 1);
        })
        .addState("Storage", [](cJSON *st) {
            uint64_t flash_total = LittleFS.totalBytes();
            uint64_t flash_used = LittleFS.usedBytes();
            uint64_t flash_free = flash_total > flash_used ? flash_total - flash_used : 0;
            cJSON_AddNumberToObject(st, "flash_freeBytes", static_cast<double>(flash_free));

            uint64_t sd_free = 0;
            HAL::SharedSpiLock spi_lock;
            if (HAL::GetHAL().sdIsReady())
            {
                uint64_t sd_total = HAL::GetHAL().sdTotalBytes();
                uint64_t sd_used = HAL::GetHAL().sdUsedBytes();
                if (sd_total > sd_used)
                {
                    sd_free = sd_total - sd_used;
                }
            }
            cJSON_AddNumberToObject(st, "sd_freeBytes", static_cast<double>(sd_free));
        })
        .addState("Sensor", [](cJSON *st) {
            SHT40_DATA data = HAL::GetHAL().envRead();
            cJSON_AddNumberToObject(st, "temp", round(data.temperature * 10.0) / 10.0);
            cJSON_AddNumberToObject(st, "humidity", round(data.humidity * 10.0) / 10.0);
        })
        .addState("SD", [](cJSON *st) {
            cJSON_AddBoolToObject(st, "is_inserted", HAL::GetHAL().sdIsInserted());
            cJSON_AddBoolToObject(st, "is_mounted", HAL::GetHAL().sdIsMounted());
            cJSON_AddBoolToObject(st, "is_ready", HAL::GetHAL().sdIsReady());
        })
        .addState("Devicestatus", [status](cJSON *st) {
            cJSON_AddNumberToObject(st, "status", status);
        });

    builder
        .addMethod("Buttons", "SetButtons", "Control", [](cJSON *params) {
            cJSON *left = cJSON_CreateObject();
            cJSON_AddItemToObject(params, "left", left);
            cJSON_AddStringToObject(left, "description", "Left key state,0=enable,1=disable");
            cJSON_AddStringToObject(left, "type", "number");

            cJSON *right = cJSON_CreateObject();
            cJSON_AddItemToObject(params, "right", right);
            cJSON_AddStringToObject(right, "description", "Right key state,0=enable,1=disable");
            cJSON_AddStringToObject(right, "type", "number");
        })
        .addMethod("DataAccess", "SetInterval", "Screen refresh interval", [](cJSON *params) {
            cJSON *interval = cJSON_CreateObject();
            cJSON_AddItemToObject(params, "interval", interval);
            cJSON_AddStringToObject(interval, "description", "refresh interval(second),minimum: 1");
            cJSON_AddStringToObject(interval, "type", "number");
        });

    int msg_id = builder.send();
    if (msg_id >= 0)
    {
        Log.infoln(F("[app_sensecraft] IOT report sent."));
    }
    else
    {
        Log.errorln(F("[app_sensecraft] IOT report failed."));
    }
    return msg_id;
}

void firstReportCb(void *arg)
{
    if (!g_first_image_res_received)
    {
        esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_SCREEN_REFRESH, nullptr, 1, portMAX_DELAY);
    }
    esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_IOT_REPORT, nullptr, 0, portMAX_DELAY);
}

void imageRetryCb(void *arg)
{
    if (!g_image_res_received)
    {
        stopTimer(g_refresh_timer);
        esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_SCREEN_REFRESH, nullptr, 1, portMAX_DELAY);
    }
}

void requestUpdateCb(void *arg)
{
    if (app_view_is_refreshing() || app_wifi_is_provisioning() || app_user_action_is_active())
    {
        Log.verboseln(F("[app_sensecraft] Skip image refresh while device is busy."));
        return;
    }

    const int image_count = GetImageCount();
    app_gallery_image_ref_t image = {};
    const bool has_image = image_count > 1
        ? app_gallery_select_next(&image)
        : app_gallery_current(&image);

    if (has_image)
    {
        if (!image.is_dynamic)
        {
            if (image_count > 1)
            {
                esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, nullptr, 1, portMAX_DELAY);
            }
            return;
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, nullptr, 1, portMAX_DELAY);
            return;
        }

        if (app_download_refresh_image(image.id))
        {
            Log.infoln(F("[app_sensecraft] Refresh dynamic image: id=%s order=%d"),
                       image.id,
                       image.order);
            return;
        }

        request_image_resource(image.id);
        return;
    }

    if (IsGalleryContent())
    {
        Log.warningln(F("[app_sensecraft] Refresh timer fired but no playable image is available."));
        return;
    }

    esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_SCREEN_REFRESH, nullptr, 1, portMAX_DELAY);
}

void bindRetryCb(void *arg)
{
    xEventGroupSetBits(g_events, EVT_BIND_RETRY);
}

void bindPollCb(void *arg)
{
    xEventGroupSetBits(g_events, EVT_BIND_POLL);
}

void iotTimerCb(void *arg)
{
    if (!app_wifi_is_provisioning() && g_state == SenseCraftState::Online)
    {
        esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_IOT_REPORT, nullptr, 0, portMAX_DELAY);
    }
}

void configReportCb(void *arg)
{
    if (!g_config_report_pending.load() || app_wifi_is_provisioning() || g_state != SenseCraftState::Online)
    {
        return;
    }

    esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_IOT_REPORT, nullptr, 0, portMAX_DELAY);
}

void processEvents(EventBits_t bits)
{
    if (bits & (EVT_ENSURE_SESSION | EVT_BIND_RETRY | EVT_BIND_POLL))
    {
        ensureSession();
    }

    if (bits & EVT_MQTT_CONNECT)
    {
        connectMqtt();
    }

    if (bits & EVT_SCREEN_REFRESH)
    {
        handleImageRefreshReq();
    }

    if (bits & EVT_IOT_REPORT)
    {
        const bool config_report_pending = g_config_report_pending.load();
        const uint32_t config_report_generation = g_config_report_generation.load();
        int msg_id = publishIotReport();
        if (msg_id >= 0 && config_report_pending)
        {
            g_config_report_msg_generation.store(config_report_generation);
            g_config_report_msg_id.store(msg_id);
        }
    }
}

void sensecraftTask(void *param)
{
    while (true)
    {
        if (!g_started)
        {
            Log.infoln(F("[app_sensecraft] SenseCraft task started."));
            g_started = true;
        }

        EventBits_t bits = xEventGroupWaitBits(
            g_events,
            EVT_ENSURE_SESSION |
                EVT_BIND_RETRY |
                EVT_BIND_POLL |
                EVT_MQTT_CONNECT |
                EVT_SCREEN_REFRESH |
                EVT_IOT_REPORT,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY);

        processEvents(bits);
    }
}

void ctrlEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id)
    {
    case SENSECRAFT_EVENT_START:
    case SENSECRAFT_EVENT_RESUME:
        Log.verboseln(F("[app_sensecraft] Network ready event: %d"), id);
        g_network_ready = true;
        setState(SenseCraftState::NeedSession);
        if (g_mqtt_cfg.host.isEmpty())
        {
            xEventGroupSetBits(g_events, EVT_ENSURE_SESSION);
        }
        else
        {
            xEventGroupSetBits(g_events, EVT_MQTT_CONNECT);
        }
        break;

    case SENSECRAFT_EVENT_STOP:
        Log.verboseln(F("[app_sensecraft] Network stop event."));
        g_network_ready = false;
        setState(SenseCraftState::Offline);
        releaseCloudWaitPower();
        g_waiting_for_content = false;
        stopMqtt(false);
        break;

    case MQTT_EVENT_SCREEN_REFRESH:
        xEventGroupSetBits(g_events, EVT_SCREEN_REFRESH);
        break;

    case MQTT_EVENT_IOT_REPORT:
        xEventGroupSetBits(g_events, EVT_IOT_REPORT);
        break;

    case DEVICE_EVENT_DEEPSLEEP:
        EnterDeepSleep();
        break;

    default:
        break;
    }
}

void mqttEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (static_cast<esp_mqtt_event_id_t>(event_id))
    {
    case MQTT_EVENT_CONNECTED:
        Log.infoln(F("[app_sensecraft] MQTT connected."));
        setState(SenseCraftState::Online);
        g_first_image_res_received = false;
        g_waiting_for_content = false;

        if (IsTimerWakeup() && IsGalleryContent())
        {
            app_gallery_image_ref_t image = {};
            if (app_gallery_current(&image))
            {
                g_refresh_target_id = image.id;
                Log.infoln(F("[app_sensecraft] Timer wakeup refresh target: id=%s order=%d"),
                           image.id,
                           image.order);
            }
        }

        if (!g_mqtt_cfg.subscribeTopic.isEmpty())
        {
            esp_mqtt_client_subscribe(g_mqtt, g_mqtt_cfg.subscribeTopic.c_str(), 1);
        }

        stopTimer(g_first_report_timer);
        stopTimer(g_iot_timer);
        esp_timer_start_once(g_first_report_timer, FIRST_REPORT_DELAY_US);
        esp_timer_start_periodic(g_iot_timer, IOT_REPORT_INTERVAL_US);
        break;

    case MQTT_EVENT_DISCONNECTED:
        Log.warningln(F("[app_sensecraft] MQTT disconnected."));
        if (g_state == SenseCraftState::Online || g_state == SenseCraftState::MqttConnecting)
        {
            setState(g_network_ready ? SenseCraftState::MqttConnecting : SenseCraftState::Offline);
        }
        stopTimer(g_iot_timer);
        esp_event_post(WIFI_APP_EVENT_BASE, APP_MQTT_DISCONNECTED, nullptr, 0, pdMS_TO_TICKS(10000));
        break;

    case MQTT_EVENT_SUBSCRIBED:
        Log.verboseln(F("[app_sensecraft] MQTT subscribed, msg_id=%d."), event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        Log.verboseln(F("[app_sensecraft] MQTT unsubscribed, msg_id=%d."), event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        Log.verboseln(F("[app_sensecraft] MQTT published, msg_id=%d."), event->msg_id);
        if (event->msg_id == g_config_report_msg_id.load() &&
            g_config_report_msg_generation.load() == g_config_report_generation.load())
        {
            g_config_report_pending.store(false);
            g_config_report_msg_id.store(-1);
        }
        xSemaphoreTake(g_sleep_mutex, portMAX_DELAY);
        if (event->msg_id == g_sleep_msg_id)
        {
            xEventGroupSetBits(g_events, EVT_SLEEP_PUBLISHED);
        }
        xSemaphoreGive(g_sleep_mutex);
        break;

    case MQTT_EVENT_DATA:
        handleMqttData(event);
        break;

    case MQTT_EVENT_ERROR:
        Log.errorln(F("[app_sensecraft] MQTT error."));
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            Log.errorln(F("[app_sensecraft] MQTT refused: 0x%x"), event->error_handle->connect_return_code);
            g_mqtt_cfg = MqttConfig();
            setState(g_network_ready ? SenseCraftState::NeedSession : SenseCraftState::Offline);
            xEventGroupSetBits(g_events, EVT_ENSURE_SESSION);
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            Log.errorln(F("[app_sensecraft] MQTT transport err=0x%x, sock=%s"),
                        event->error_handle->esp_tls_last_esp_err,
                        strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        break;
    }
}
} // namespace

bool app_publish_sleep_wait(uint32_t timeout_ms)
{
    if (!g_events || g_state != SenseCraftState::Online)
    {
        return false;
    }

    xSemaphoreTake(g_sleep_mutex, portMAX_DELAY);
    g_sleep_msg_id = -1;
    xEventGroupClearBits(g_events, EVT_SLEEP_PUBLISHED);

    int msg_id = publishIotReport(DEVICE_SLEEP);
    if (msg_id >= 0)
    {
        g_sleep_msg_id = msg_id;
    }
    xSemaphoreGive(g_sleep_mutex);

    if (msg_id < 0)
    {
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(
        g_events,
        EVT_SLEEP_PUBLISHED,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));
    xSemaphoreTake(g_sleep_mutex, portMAX_DELAY);
    g_sleep_msg_id = -1;
    xSemaphoreGive(g_sleep_mutex);
    return (bits & EVT_SLEEP_PUBLISHED) != 0;
}

void request_image_resource(const char *image_id)
{
    g_refresh_target_id = image_id ? image_id : "";
    esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_SCREEN_REFRESH, nullptr, 1, portMAX_DELAY);
}

void request_update_timer_start()
{
    stopTimer(g_refresh_timer);

    uint32_t interval_seconds = GetDeepSleepInterval();
    if (interval_seconds == 0)
    {
        interval_seconds = 1;
        SetDeepSleepInterval(interval_seconds);
        Log.warningln(F("[app_sensecraft] Refresh interval 0s, fallback to 1s."));
    }

    esp_err_t err = esp_timer_start_periodic(g_refresh_timer, static_cast<uint64_t>(interval_seconds) * 1000ULL * 1000ULL);
    if (err == ESP_OK)
    {
        Log.infoln(F("[app_sensecraft] Refresh timer started, interval=%us."), interval_seconds);
    }
    else
    {
        Log.errorln(F("[app_sensecraft] Refresh timer start failed: %s"), esp_err_to_name(err));
    }
}

void request_update_timer_stop()
{
    stopTimer(g_refresh_timer);
}

void imgRefreshRes(const char *version, bool apply, int download_per, const char *image_id, int image_index, int image_total)
{
    if (g_state != SenseCraftState::Online)
    {
        return;
    }

    cJSON *doc = cJSON_CreateObject();
    if (!doc)
    {
        return;
    }

    cJSON_AddStringToObject(doc, "version", Protocol_version);
    cJSON_AddStringToObject(doc, "session_id", sessionId().c_str());
    cJSON_AddStringToObject(doc, "type", "img_flash_res");
    cJSON_AddNumberToObject(doc, "timestamp", static_cast<double>(timestampMs()));

    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(doc, "data", data);
    cJSON_AddStringToObject(data, "version", version);
    cJSON_AddStringToObject(data, "img_id", image_id);
    cJSON_AddBoolToObject(data, "img_apply", apply);
    cJSON_AddNumberToObject(data, "img_progress", download_per);
    cJSON_AddNumberToObject(data, "img_total", image_total);
    cJSON_AddNumberToObject(data, "img_index", image_index);

    if (publish(doc) >= 0)
    {
        Log.verboseln(F("[app_sensecraft] Image refresh result sent, version=%s."), version);
    }
    else
    {
        Log.errorln(F("[app_sensecraft] Image refresh result failed."));
    }

    cJSON_Delete(doc);
}

void app_task_init()
{
    if (g_registered)
    {
        Log.warningln(F("[app_sensecraft] SenseCraft task already running."));
        return;
    }

    createTimer("bind_retry", bindRetryCb, &g_bind_retry_timer);
    createTimer("bind_poll", bindPollCb, &g_bind_poll_timer);
    createTimer("first_report", firstReportCb, &g_first_report_timer);
    createTimer("image_retry", imageRetryCb, &g_image_retry_timer);
    createTimer("iot_report", iotTimerCb, &g_iot_timer);
    createTimer("config_report", configReportCb, &g_config_report_timer);
    createTimer("refresh_req", requestUpdateCb, &g_refresh_timer);

    g_events = xEventGroupCreate();
    g_sleep_mutex = xSemaphoreCreateMutex();

    esp_event_handler_register(CTRL_EVENT_BASE, SENSECRAFT_EVENT_START, ctrlEventHandler, nullptr);
    esp_event_handler_register(CTRL_EVENT_BASE, SENSECRAFT_EVENT_RESUME, ctrlEventHandler, nullptr);
    esp_event_handler_register(CTRL_EVENT_BASE, SENSECRAFT_EVENT_STOP, ctrlEventHandler, nullptr);
    esp_event_handler_register(CTRL_EVENT_BASE, MQTT_EVENT_SCREEN_REFRESH, ctrlEventHandler, nullptr);
    esp_event_handler_register(CTRL_EVENT_BASE, MQTT_EVENT_IOT_REPORT, ctrlEventHandler, nullptr);
    esp_event_handler_register(CTRL_EVENT_BASE, DEVICE_EVENT_DEEPSLEEP, ctrlEventHandler, nullptr);

    mbedtls_platform_set_calloc_free(ps_calloc, free);

    BaseType_t created = xTaskCreate(
        sensecraftTask,
        "app_sensecraft",
        SENSECRAFT_TASK_STACK_SIZE,
        nullptr,
        SENSECRAFT_TASK_PRIORITY,
        &g_task);

    if (created != pdPASS)
    {
        g_task = nullptr;
        Log.errorln(F("[app_sensecraft] Failed to create task."));
        return;
    }

    g_registered = true;
}

void app_sensecraft_request_iot_report()
{
    g_config_report_generation.fetch_add(1);
    g_config_report_pending.store(true);
    scheduleConfigReport();
}
