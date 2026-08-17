#include "APP/app_device_info.h"
#include "APP/app_gallery.h"
#include "APP/app_power_manager.h"
#include "APP/app_sensecraft.h"
#include "APP/app_user_action.h"
#include "APP/app_view.h"
#include "hal/hal.h"
#include "hal/hal_indicator.h"

#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <atomic>
#include <vector>
#include <algorithm>
#include <memory>
#include <new>
#include <LittleFS.h>
#include <RtcPCF8563.h>
#include "ArduinoLog.h"
#include "cJSON.h"

#include "app_config.h"

#include "APP/app_events.h"

#define NVS_NAMESPACE "hmi_config"
#define NVS_KEY_DEVICE_BOUND "devBound"
#define NVS_KEY_IMAGE_COUNT "img_count"
#define NVS_KEY_IMAGE_VERSION "imgVer"
#define NVS_KEY_CURRENT_IMAGE_ID "currentImgId"
#define NVS_KEY_CURRENT_IMAGE_ORDER "currentImgOrder"
#define NVS_KEY_IS_IMG_IN "isimg"
#define NVS_KEY_ALBUM_VERSION "albumVersion"
#define NVS_KEY_DEVICE_MODE "deviceMode"
#define NVS_KEY_SLEEP_INTERVAL "sleepInterval"
#define NVS_KEY_DISABLE_SLEEP "disableSleep"
#define NVS_KEY_WIFI_SSID_PREFIX "wifi_ssid_"
#define NVS_KEY_WIFI_PASS_PREFIX "wifi_pass_"
#define MAX_WIFI_CREDENTIALS 5

#define DEBOUNCE_DELAY_MS 500

#define LOW_BATTERY_AUTO_SLEEP_PERCENT 3.0f
#define LOW_BATTERY_CHECK_INTERVAL_S 60
#define LOW_BATTERY_SLEEP_DELAY_MS 15000

static const int MAINTENANCE_START_HOUR = 4;
static const int MAINTENANCE_START_MINUTE = 30;
static const int MAINTENANCE_END_HOUR = 4;
static const int MAINTENANCE_END_MINUTE = 31;
static const int MAINTENANCE_WINDOW_SECONDS = 30;
static const uint32_t SLEEP_REPORT_TIMEOUT_MS = 3000;
static const uint32_t DEVICE_INFO_TASK_STACK_SIZE = 6144;
static const UBaseType_t DEVICE_INFO_TASK_PRIORITY = tskIDLE_PRIORITY + 1;

static std::atomic<bool> local_gallery_session{false};
static std::atomic<bool> timer_wakeup{false};
static std::atomic<bool> deep_sleep_pending{false};
static SemaphoreHandle_t runtimeDataMutex = xSemaphoreCreateMutex();
static String cloud_token;
static int activation_code = -1;

typedef enum
{
    DEVCFG_TYPE_DEVICE_MODE,
    DEVCFG_TYPE_DEVICE_BOUND,
    DEVCFG_TYPE_IMAGE_VERSION,
    DEVCFG_TYPE_IMAGE_COUNT,
    DEVCFG_TYPE_IS_IMAGE_IN,
    DEVCFG_TYPE_CURRENT_IMAGE_ID,
    DEVCFG_TYPE_CURRENT_IMAGE_ORDER,
    DEVCFG_TYPE_ALBUM_VERSION,
    DEVCFG_TYPE_SLEEP_INTERVAL,
    DEVCFG_TYPE_WIFI_CREDENTIALS,
    DEVCFG_TYPE_DISABLE_SLEEP,
    DEVCFG_TYPE_MAX
} devicecfg_type_t;

#define EVENT_BIT(T) (BIT0 << T)
#define EVENT_DEVICECFG_CHANGE BIT0
#define EVENT_TIMER_1S BIT1

typedef struct
{
    devicecfg_type_t type;
    SemaphoreHandle_t mutex;
    union
    {
        bool bool_val;
        int int_val;
        uint32_t uint32_val;
        char *str_val;
    } current, last;
    esp_timer_handle_t timer_handle;
} DeviceCfg;

static Preferences preferences;
static DeviceCfg *g_deviceCfgs = NULL;
static EventGroupHandle_t g_eg_task_wakeup;
static EventGroupHandle_t g_eg_devicecfg_change;
static std::vector<std::pair<String, String>> g_wifi_credentials;

static uint8_t __wakeup_button = 9;
static esp_timer_handle_t __timer_every_1s;
esp_timer_handle_t __wakerup_action_timer;

static void _timer_cb_debounce(void *arg);
static bool nvs_set_device_mode(bool mode);
static bool nvs_get_device_mode(bool &mode);
static bool nvs_set_device_bound(int state);
static bool nvs_get_device_bound(int &state);
static bool nvs_set_image_version(const String &version);
static bool nvs_get_image_info(String &version, int &count);
static bool nvs_set_image_count(int count);
static bool nvs_get_image_count(int &count);
static bool nvs_set_is_image_in(bool is_image_in);
static bool nvs_get_is_image_in(bool &is_image_in);
static bool nvs_set_current_image_id(const String &id);
static bool nvs_get_current_image_id(String &id);
static bool nvs_set_current_image_order(int order);
static bool nvs_get_current_image_order(int &order);
static bool nvs_set_album_version(const String &version);
static bool nvs_get_album_version(String &version);
static bool nvs_set_sleep_interval(uint32_t interval);
static bool nvs_get_sleep_interval(uint32_t &interval);
static bool nvs_set_disable_sleep(uint32_t disable);
static bool nvs_get_disable_sleep(uint32_t &disable);
static bool nvs_clear_all();
static bool is_local_gallery_session_enabled();
static void set_local_gallery_session_enabled(bool enabled);

static bool imageExistsOnAnyStorage(const String &path)
{
    if (path.isEmpty())
    {
        return false;
    }

    if (LittleFS.exists(path.c_str()))
    {
        return true;
    }

    HAL::SharedSpiLock spiLock;
    if (HAL::GetHAL().sdIsReady() && HAL::GetHAL().sdExists(path.c_str()))
    {
        return true;
    }

    return false;
}

static String deriveImagePathFromManifestEntry(const cJSON *image)
{
    if (image == nullptr || !cJSON_IsObject(image))
    {
        return String();
    }

    const cJSON *idItem = cJSON_GetObjectItemCaseSensitive(image, "id");
    const char *imgId = (cJSON_IsString(idItem) && idItem->valuestring && idItem->valuestring[0] != '\0') ? idItem->valuestring : nullptr;
    if (imgId)
    {
        const String pngPath = "/" + String(imgId) + ".png";
        if (imageExistsOnAnyStorage(pngPath))
        {
            return pngPath;
        }

        const String bmpPath = "/" + String(imgId) + ".bmp";
        if (imageExistsOnAnyStorage(bmpPath))
        {
            return bmpPath;
        }

        return bmpPath;
    }

    const cJSON *fileNameItem = cJSON_GetObjectItemCaseSensitive(image, "file_name");
    const char *fileName = (cJSON_IsString(fileNameItem) && fileNameItem->valuestring && fileNameItem->valuestring[0] != '\0') ? fileNameItem->valuestring : nullptr;
    if (fileName)
    {
        String fileNameStr = fileName;
        if (!fileNameStr.startsWith("/"))
        {
            fileNameStr = "/" + fileNameStr;
        }
        return fileNameStr;
    }

    const cJSON *urlItem = cJSON_GetObjectItemCaseSensitive(image, "url");
    const char *url = (cJSON_IsString(urlItem) && urlItem->valuestring && urlItem->valuestring[0] != '\0') ? urlItem->valuestring : nullptr;
    if (url)
    {
        String urlStr = url;
        int queryIndex = urlStr.indexOf('?');
        if (queryIndex > 0)
        {
            urlStr = urlStr.substring(0, queryIndex);
        }

        int slashIndex = urlStr.lastIndexOf('/');
        if (slashIndex != -1 && slashIndex + 1 < urlStr.length())
        {
            String fileNameStr = urlStr.substring(slashIndex + 1);
            if (!fileNameStr.startsWith("/"))
            {
                fileNameStr = "/" + fileNameStr;
            }
            return fileNameStr;
        }
    }

    return String();
}

static void removeAllBmpFromLittleFs()
{
    File root = LittleFS.open("/");
    if (!root)
    {
        Log.warningln("[app_device_info] Failed to open LittleFS root for image cleanup.");
        return;
    }
    if (!root.isDirectory())
    {
        Log.warningln("[app_device_info] LittleFS root is not a directory.");
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (!file.isDirectory())
        {
            String fileName = String(file.name());
            if (fileName.endsWith(".bmp") || fileName.endsWith(".png"))
            {
                String fullPath = fileName.startsWith("/") ? fileName : "/" + fileName;
                if (LittleFS.remove(fullPath.c_str()))
                {
                    Log.infoln("[app_device_info] Deleted LittleFS image: %s", fullPath.c_str());
                }
                else
                {
                    Log.errorln("[app_device_info] Failed to delete LittleFS image: %s", fullPath.c_str());
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}

static void removeAllBmpFromSd()
{
    HAL::SharedSpiLock spiLock;
    if (!HAL::GetHAL().sdEnsureReady())
    {
        return;
    }

    File root = HAL::GetHAL().sdOpen("/");
    if (!root)
    {
        Log.warningln("[app_device_info] Failed to open MicroSD root for image cleanup.");
        return;
    }
    if (!root.isDirectory())
    {
        Log.warningln("[app_device_info] MicroSD root is not a directory.");
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (!file.isDirectory())
        {
            String fileName = String(file.name());
            if (fileName.endsWith(".bmp") || fileName.endsWith(".png"))
            {
                String fullPath = fileName.startsWith("/") ? fileName : "/" + fileName;
                if (HAL::GetHAL().sdRemove(fullPath.c_str()))
                {
                    Log.infoln("[app_device_info] Deleted MicroSD image: %s", fullPath.c_str());
                }
                else
                {
                    Log.errorln("[app_device_info] Failed to delete MicroSD image: %s", fullPath.c_str());
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}

static void cleanupImagesUsingManifest()
{
    const char *manifestPath = "/manifest.json";
    bool sdAvailable = HAL::GetHAL().sdEnsureReady();

    Log.infoln("[app_device_info] Cleaning up image assets based on manifest.json");

    if (LittleFS.exists(manifestPath))
    {
        File manifestFile = LittleFS.open(manifestPath, FILE_READ);
        if (!manifestFile)
        {
            Log.errorln("[app_device_info] Failed to open manifest.json for reading.");
            if (LittleFS.remove(manifestPath))
            {
                Log.verboseln("[app_device_info] Deleted manifest.json after open failure.");
            }
            else
            {
                Log.errorln("[app_device_info] Failed to delete manifest.json after open failure.");
            }
        }
        else
        {
            size_t fileSize = manifestFile.size();
            if (fileSize == 0)
            {
                Log.warningln("[app_device_info] manifest.json is empty. Removing file.");
                manifestFile.close();
                if (!LittleFS.remove(manifestPath))
                {
                    Log.errorln("[app_device_info] Failed to delete empty manifest.json");
                }
            }
            else if (MANIFEST_MAX_SIZE_BYTES > 0 && fileSize > MANIFEST_MAX_SIZE_BYTES)
            {
                Log.warningln("[app_device_info] manifest.json is too large (%u bytes), skipping parse and deleting it.",
                              static_cast<unsigned>(fileSize));
                manifestFile.close();
                if (!LittleFS.remove(manifestPath))
                {
                    Log.errorln("[app_device_info] Failed to delete oversized manifest.json");
                }
            }
            else
            {
                std::unique_ptr<char[]> buffer(new (std::nothrow) char[fileSize + 1]);
                if (!buffer)
                {
                    Log.errorln("[app_device_info] Failed to allocate buffer for manifest contents.");
                    manifestFile.close();
                    if (LittleFS.remove(manifestPath))
                    {
                        Log.verboseln("[app_device_info] Deleted manifest.json after allocation failure.");
                    }
                    else
                    {
                        Log.errorln("[app_device_info] Failed to delete manifest.json after allocation failure.");
                    }
                }
                else
                {
                    size_t readBytes = manifestFile.readBytes(buffer.get(), fileSize);
                    buffer[fileSize] = '\0';
                    manifestFile.close();

                    if (readBytes != fileSize)
                    {
                        Log.warningln("[app_device_info] Unexpected manifest read size. Expected %u, got %u.", (unsigned)fileSize, (unsigned)readBytes);
                    }

                    cJSON *doc = cJSON_Parse(buffer.get());
                    if (doc == nullptr)
                    {
                        const char *errorPtr = cJSON_GetErrorPtr();
                        Log.errorln("[app_device_info] Failed to parse manifest.json during cleanup: %s", errorPtr ? errorPtr : "unknown error");
                        if (LittleFS.remove(manifestPath))
                        {
                            Log.verboseln("[app_device_info] Deleted manifest.json after parse failure.");
                        }
                        else
                        {
                            Log.errorln("[app_device_info] Failed to delete manifest.json after parse failure.");
                        }
                    }
                    else
                    {
                        const cJSON *images = cJSON_GetObjectItemCaseSensitive(doc, "images");
                        if (!cJSON_IsArray(images))
                        {
                            Log.warningln("[app_device_info] manifest.json does not contain an 'images' array.");
                        }
                        else
                        {
                            int removedCount = 0;
                            const cJSON *image = images->child;
                            while (image != nullptr)
                            {
                                String imagePath = deriveImagePathFromManifestEntry(image);
                                if (imagePath.isEmpty())
                                {
                                    Log.warningln("[app_device_info] Skipping manifest entry without resolvable image path.");
                                    image = image->next;
                                    continue;
                                }

                                bool removedFromLittleFs = false;
                                if (LittleFS.exists(imagePath.c_str()))
                                {
                                    if (LittleFS.remove(imagePath.c_str()))
                                    {
                                        Log.verboseln("[app_device_info] Deleted LittleFS image: %s", imagePath.c_str());
                                        removedFromLittleFs = true;
                                    }
                                    else
                                    {
                                        Log.errorln("[app_device_info] Failed to delete LittleFS image: %s", imagePath.c_str());
                                    }
                                }

                                bool removedFromSd = false;
                                if (sdAvailable)
                                {
                                    HAL::SharedSpiLock spiLock;
                                    if (HAL::GetHAL().sdIsReady() && HAL::GetHAL().sdExists(imagePath.c_str()))
                                    {
                                        if (HAL::GetHAL().sdRemove(imagePath.c_str()))
                                        {
                                            Log.verboseln("[app_device_info] Deleted MicroSD image: %s", imagePath.c_str());
                                            removedFromSd = true;
                                        }
                                        else
                                        {
                                            Log.errorln("[app_device_info] Failed to delete MicroSD image: %s", imagePath.c_str());
                                        }
                                    }
                                }

                                if (removedFromLittleFs || removedFromSd)
                                {
                                    removedCount++;
                                }
                                else
                                {
                                    Log.warningln("[app_device_info] Image file not found during cleanup: %s", imagePath.c_str());
                                }

                                image = image->next;
                            }
                            Log.infoln("[app_device_info] Removed %d images referenced in manifest.json", removedCount);
                        }

                        cJSON_Delete(doc);

                        if (!LittleFS.remove(manifestPath))
                        {
                            Log.errorln("[app_device_info] Failed to delete manifest.json after cleanup.");
                        }
                        else
                        {
                            Log.verboseln("[app_device_info] Deleted manifest.json after cleanup.");
                        }
                    }
                }
            }
        }
    }
    else
    {
        Log.warningln("[app_device_info] manifest.json not found; falling back to full image cleanup.");
    }

    removeAllBmpFromLittleFs();
    removeAllBmpFromSd();
    Log.infoln("[app_device_info] Image cleanup finished.");
}

AppContentMode GetContentMode()
{
    bool gallery = false;
    nvs_get_device_mode(gallery);
    return gallery ? AppContentMode::Gallery : AppContentMode::Dashboard;
}

esp_err_t SetContentMode(AppContentMode mode)
{
    return nvs_set_device_mode(mode == AppContentMode::Gallery) ? ESP_OK : ESP_FAIL;
}

bool IsGalleryContent()
{
    return GetContentMode() == AppContentMode::Gallery;
}

bool IsLocalGallerySession()
{
    return is_local_gallery_session_enabled();
}

esp_err_t SetLocalGallerySession(bool enabled)
{
    set_local_gallery_session_enabled(enabled);
    return ESP_OK;
}

String GetContentVersion()
{
    String version;
    int count = 0;
    nvs_get_image_info(version, count);
    if (version.isEmpty())
    {
        return "0.0";
    }
    return version;
}

esp_err_t SetContentVersion(const String &version)
{
    return nvs_set_image_version(version) ? ESP_OK : ESP_FAIL;
}

int GetImageCount()
{
    int count = 0;
    nvs_get_image_count(count);
    return count;
}

esp_err_t SetImageCount(int count)
{
    return nvs_set_image_count(count) ? ESP_OK : ESP_FAIL;
}

bool HasImage()
{
    bool has_image = false;
    nvs_get_is_image_in(has_image);
    return has_image;
}

esp_err_t SetHasImage(bool has_image)
{
    return nvs_set_is_image_in(has_image) ? ESP_OK : ESP_FAIL;
}

String GetCurrentImageId()
{
    String id;
    nvs_get_current_image_id(id);
    return id;
}

esp_err_t SetCurrentImageId(const String &id)
{
    return nvs_set_current_image_id(id) ? ESP_OK : ESP_FAIL;
}

int GetCurrentImageOrder()
{
    int order = 0;
    nvs_get_current_image_order(order);
    return order;
}

esp_err_t SetCurrentImageOrder(int order)
{
    return nvs_set_current_image_order(order) ? ESP_OK : ESP_FAIL;
}

String GetAlbumVersion()
{
    String version;
    nvs_get_album_version(version);
    return version;
}

esp_err_t SetAlbumVersion(const String &version)
{
    return nvs_set_album_version(version) ? ESP_OK : ESP_FAIL;
}

uint32_t GetDeepSleepInterval()
{
    uint32_t interval_s = 0;
    nvs_get_sleep_interval(interval_s);
    return interval_s;
}

esp_err_t SetDeepSleepInterval(uint32_t seconds)
{
    return nvs_set_sleep_interval(seconds) ? ESP_OK : ESP_FAIL;
}

bool GetDeepSleepEnabled()
{
    uint32_t disabled = 0;
    nvs_get_disable_sleep(disabled);
    return disabled == 0;
}

esp_err_t SetDeepSleepEnabled(bool enabled)
{
    bool ok = nvs_set_disable_sleep(enabled ? 0 : 1);
    if (ok)
    {
        app_power_manager_set_enabled(enabled);
    }
    return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t SetCloudToken(const String &token)
{
    xSemaphoreTake(runtimeDataMutex, portMAX_DELAY);
    cloud_token = token;
    xSemaphoreGive(runtimeDataMutex);
    return ESP_OK;
}

String GetCloudToken()
{
    xSemaphoreTake(runtimeDataMutex, portMAX_DELAY);
    String token = cloud_token;
    xSemaphoreGive(runtimeDataMutex);
    return token;
}

bool GetBindState()
{
    int state = 0;
    nvs_get_device_bound(state);
    return state == DeviceReset::DEVICE_BIND;
}

esp_err_t SetBindState(bool bound)
{
    return nvs_set_device_bound(bound ? DeviceReset::DEVICE_BIND : DeviceReset::DEVICE_NETWORK) ? ESP_OK : ESP_FAIL;
}

esp_err_t SetActivationCode(int code)
{
    xSemaphoreTake(runtimeDataMutex, portMAX_DELAY);
    activation_code = code;
    xSemaphoreGive(runtimeDataMutex);
    return ESP_OK;
}

int GetActivationCode()
{
    xSemaphoreTake(runtimeDataMutex, portMAX_DELAY);
    int code = activation_code;
    xSemaphoreGive(runtimeDataMutex);
    return code;
}

static bool is_local_gallery_session_enabled()
{
    return local_gallery_session.load();
}

static void set_local_gallery_session_enabled(bool enabled)
{
    local_gallery_session.store(enabled);
}

bool IsTimerWakeup()
{
    return timer_wakeup.load();
}

esp_err_t SetTimerWakeup(bool enabled)
{
    timer_wakeup.store(enabled);
    return ESP_OK;
}

void MarkUserWakeup()
{
    SetTimerWakeup(false);
}

void EnterDeepSleep()
{
#if (RETERMINAL_DEEPSLEEP_DISABLE == 0)
    if(HAL::GetHAL().pmicIsCharging())return;
#endif
    Log.verboseln("EnterDeepSleep decision");
    if (app_power_manager_has_blocker())
    {
        Log.infoln("[app_device_info] Deep sleep blocked by power owner mask=0x%08x",
                   app_power_manager_owner_mask());
        return;
    }

    bool expected = false;
    if (!deep_sleep_pending.compare_exchange_strong(expected, true))
    {
        Log.verboseln("[app_device_info] Deep sleep is already pending.");
        return;
    }

    if (app_publish_sleep_wait(SLEEP_REPORT_TIMEOUT_MS))
    {
        Log.infoln("[app_device_info] Sleep status acknowledged by MQTT.");
    }
    else
    {
        Log.warningln("[app_device_info] Sleep status was not acknowledged, entering sleep.");
    }

    uint64_t wakeup_sleep_seconds = 0;
    RtcDateTime now_dt;
    if (!HAL::GetHAL().rtcReadTime(now_dt))
    {
        uint32_t _sleep_interval;
        nvs_get_sleep_interval(_sleep_interval);
        wakeup_sleep_seconds = static_cast<uint64_t>(_sleep_interval - 20);
    }
    else
    {
        RtcDateTime maintenance_start_dt(now_dt.Year(), now_dt.Month(), now_dt.Day(), MAINTENANCE_START_HOUR, MAINTENANCE_START_MINUTE, 0);
        RtcDateTime maintenance_end_dt(now_dt.Year(), now_dt.Month(), now_dt.Day(), MAINTENANCE_END_HOUR, MAINTENANCE_END_MINUTE, 0);

        if (now_dt >= maintenance_start_dt && now_dt < maintenance_start_dt + MAINTENANCE_WINDOW_SECONDS)
        {
            wakeup_sleep_seconds = static_cast<uint64_t>(maintenance_end_dt.TotalSeconds()) - now_dt.TotalSeconds();
            Log.infoln("Wake up during maintenance window, sleeping for %u seconds.", wakeup_sleep_seconds);
        }
        else
        {
            uint32_t _sleep_interval;
            nvs_get_sleep_interval(_sleep_interval);
            RtcDateTime next_wakeup_dt = now_dt + _sleep_interval;

            if (now_dt < maintenance_start_dt && next_wakeup_dt >= maintenance_start_dt)
            {
                wakeup_sleep_seconds = static_cast<uint64_t>(maintenance_start_dt.TotalSeconds()) - now_dt.TotalSeconds();
                Log.infoln("Regular sleep will overlap maintenance time, need to sleep for %u seconds.", wakeup_sleep_seconds);
            }
            else
            {
                wakeup_sleep_seconds = static_cast<uint64_t>(_sleep_interval - 20);
            }
        }
    }
    esp_sleep_enable_timer_wakeup(wakeup_sleep_seconds * 1000000ULL);

    HAL::GetHAL().touchEnableWakeup();

    if (app_power_manager_has_blocker())
    {
        Log.infoln("[app_device_info] Deep sleep cancelled by power owner mask=0x%08x",
                   app_power_manager_owner_mask());
        deep_sleep_pending.store(false);
        return;
    }
    Log.infoln("----> Enter deep sleep <----");
    esp_deep_sleep_start();
}

static void _trigger_update_bool(devicecfg_type_t type, bool value)
{
    DeviceCfg &cfg = g_deviceCfgs[type];
    xSemaphoreTake(cfg.mutex, portMAX_DELAY);
    cfg.current.bool_val = value;
    esp_timer_stop(cfg.timer_handle);
    esp_timer_start_once(cfg.timer_handle, DEBOUNCE_DELAY_MS * 1000);
    xSemaphoreGive(cfg.mutex);
}

static void _trigger_update_int(devicecfg_type_t type, int value)
{
    DeviceCfg &cfg = g_deviceCfgs[type];
    xSemaphoreTake(cfg.mutex, portMAX_DELAY);
    cfg.current.int_val = value;
    esp_timer_stop(cfg.timer_handle);
    esp_timer_start_once(cfg.timer_handle, DEBOUNCE_DELAY_MS * 1000);
    xSemaphoreGive(cfg.mutex);
}

static void _trigger_update_uint32(devicecfg_type_t type, uint32_t value)
{
    DeviceCfg &cfg = g_deviceCfgs[type];
    xSemaphoreTake(cfg.mutex, portMAX_DELAY);
    cfg.current.uint32_val = value;
    esp_timer_stop(cfg.timer_handle);
    esp_timer_start_once(cfg.timer_handle, DEBOUNCE_DELAY_MS * 1000);
    xSemaphoreGive(cfg.mutex);
}

static void _trigger_update_string(devicecfg_type_t type, const char *value)
{
    DeviceCfg &cfg = g_deviceCfgs[type];
    xSemaphoreTake(cfg.mutex, portMAX_DELAY);
    if (cfg.current.str_val)
        free(cfg.current.str_val);
    cfg.current.str_val = (value && strlen(value) > 0) ? strdup(value) : nullptr;
    esp_timer_stop(cfg.timer_handle);
    esp_timer_start_once(cfg.timer_handle, DEBOUNCE_DELAY_MS * 1000);
    xSemaphoreGive(cfg.mutex);
}

static void __set_wifi_credentials();

static void _timer_cb_debounce(void *arg)
{
    DeviceCfg *cfg = (DeviceCfg *)arg;
    xEventGroupSetBits(g_eg_devicecfg_change, EVENT_BIT(cfg->type));
    xEventGroupSetBits(g_eg_task_wakeup, EVENT_DEVICECFG_CHANGE);
}

static void __timer_cb_every_1s(void *arg)
{
    xEventGroupSetBits(g_eg_task_wakeup, EVENT_TIMER_1S);
}

static void __wakeup_timer_cb(void *arg)
{
    if (__wakeup_button == 2)
    {
        app_gallery_select_prev();
    }
    else if (__wakeup_button == 1)
    {
        app_gallery_select_next();
    }
    if (is_local_gallery_session_enabled())
        esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, NULL, 1, portMAX_DELAY);
}

void StartDeepSleepTimer()
{
#if (RETERMINAL_DEEPSLEEP_DISABLE == 0)
    if (HAL::GetHAL().pmicIsCharging())
    {
        return;
    }
#endif
    if (!GetDeepSleepEnabled())
    {
        return;
    }

    const uint64_t delay_us = IsTimerWakeup() ? 1000ULL : APP_POWER_ACTIVE_WINDOW_US;
    app_power_manager_schedule_sleep_check(delay_us);
}

void StopDeepSleepTimer()
{
    app_power_manager_stop_sleep_check();
}

void CancelPendingWakeupAction()
{
    if (__wakerup_action_timer != nullptr && esp_timer_is_active(__wakerup_action_timer))
    {
        esp_timer_stop(__wakerup_action_timer);
    }
}

void ResetAfterUnbind()
{
    int bound_state = 0;
    nvs_get_device_bound(bound_state);
    if (bound_state == DeviceReset::DEVICE_BIND)
    {
        cleanupImagesUsingManifest();
        nvs_clear_all();
        if (preferences.begin(NVS_NAMESPACE, false))
        {
            preferences.putInt(NVS_KEY_DEVICE_BOUND, DeviceReset::DEVICE_NETWORK);
            preferences.putBool(NVS_KEY_IS_IMG_IN, false);
            preferences.putString(NVS_KEY_CURRENT_IMAGE_ID, "");
            preferences.putInt(NVS_KEY_CURRENT_IMAGE_ORDER, 0);
            preferences.end();
        }
        esp_restart();
    }
}

static void persist_wifi_credentials_now()
{
    DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_WIFI_CREDENTIALS];

    if (cfg.timer_handle)
    {
        esp_timer_stop(cfg.timer_handle);
    }

    if (!preferences.begin(NVS_NAMESPACE, false))
    {
        Log.errorln("[app_device_info] Failed to open preferences while persisting WiFi credentials.");
        return;
    }

    __set_wifi_credentials();
    preferences.end();
}

bool SaveWifiCredential(const String &ssid, const String &password)
{
    DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_WIFI_CREDENTIALS];
    xSemaphoreTake(cfg.mutex, portMAX_DELAY);

    g_wifi_credentials.erase(
        std::remove_if(g_wifi_credentials.begin(), g_wifi_credentials.end(), [&](const std::pair<String, String> &cred)
                       { return cred.first == ssid; }),
        g_wifi_credentials.end());

    g_wifi_credentials.insert(g_wifi_credentials.begin(), {ssid, password});

    if (g_wifi_credentials.size() > MAX_WIFI_CREDENTIALS)
    {
        g_wifi_credentials.pop_back();
    }

    xSemaphoreGive(cfg.mutex);

    persist_wifi_credentials_now();
    return true;
}

bool GetWifiCredentials(std::vector<std::pair<String, String>> &credentials)
{
    DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_WIFI_CREDENTIALS];
    xSemaphoreTake(cfg.mutex, portMAX_DELAY);
    credentials = g_wifi_credentials;
    xSemaphoreGive(cfg.mutex);
    return true;
}

bool GetWifiPassword(const String &ssid, String &password)
{
    DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_WIFI_CREDENTIALS];
    xSemaphoreTake(cfg.mutex, portMAX_DELAY);
    bool found = false;
    for (size_t i = 0; i < g_wifi_credentials.size(); ++i)
    {
        if (g_wifi_credentials[i].first == ssid)
        {
            password = g_wifi_credentials[i].second;
            found = true;
            break;
        }
    }
    xSemaphoreGive(cfg.mutex);
    return found;
}

bool RemoveWifiCredential(const String &ssid)
{
    DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_WIFI_CREDENTIALS];
    xSemaphoreTake(cfg.mutex, portMAX_DELAY);

    auto old_size = g_wifi_credentials.size();
    g_wifi_credentials.erase(
        std::remove_if(g_wifi_credentials.begin(), g_wifi_credentials.end(), [&](const std::pair<String, String> &cred)
                       { return cred.first == ssid; }),
        g_wifi_credentials.end());

    bool changed = (g_wifi_credentials.size() != old_size);
    xSemaphoreGive(cfg.mutex);

    if (changed)
    {
        persist_wifi_credentials_now();
    }
    return true;
}

static bool nvs_set_device_mode(bool mode)
{
    _trigger_update_bool(DEVCFG_TYPE_DEVICE_MODE, mode);
    return true;
}
static bool nvs_get_device_mode(bool &mode)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_DEVICE_MODE].mutex, portMAX_DELAY);
    mode = g_deviceCfgs[DEVCFG_TYPE_DEVICE_MODE].current.bool_val;
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_DEVICE_MODE].mutex);
    return true;
}

static bool nvs_set_device_bound(int isBound)
{
    _trigger_update_int(DEVCFG_TYPE_DEVICE_BOUND, isBound);
    return true;
}
static bool nvs_get_device_bound(int &isBound)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_DEVICE_BOUND].mutex, portMAX_DELAY);
    isBound = g_deviceCfgs[DEVCFG_TYPE_DEVICE_BOUND].current.int_val;
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_DEVICE_BOUND].mutex);
    return true;
}

static bool nvs_set_image_version(const String &version)
{
    _trigger_update_string(DEVCFG_TYPE_IMAGE_VERSION, version.c_str());
    return true;
}

static bool nvs_set_image_count(int count)
{
    _trigger_update_int(DEVCFG_TYPE_IMAGE_COUNT, count);
    return true;
}
static bool nvs_get_image_count(int &count)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].mutex, portMAX_DELAY);
    count = g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].current.int_val;
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].mutex);
    return true;
}

static bool nvs_get_image_info(String &version, int &count)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_IMAGE_VERSION].mutex, portMAX_DELAY);
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].mutex, portMAX_DELAY);
    version = g_deviceCfgs[DEVCFG_TYPE_IMAGE_VERSION].current.str_val ? g_deviceCfgs[DEVCFG_TYPE_IMAGE_VERSION].current.str_val : "";
    count = g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].current.int_val;
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].mutex);
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_IMAGE_VERSION].mutex);
    return true;
}

static bool nvs_set_is_image_in(bool is_image_in)
{
    _trigger_update_bool(DEVCFG_TYPE_IS_IMAGE_IN, is_image_in);
    return true;
}
static bool nvs_get_is_image_in(bool &is_image_in)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_IS_IMAGE_IN].mutex, portMAX_DELAY);
    is_image_in = g_deviceCfgs[DEVCFG_TYPE_IS_IMAGE_IN].current.bool_val;
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_IS_IMAGE_IN].mutex);
    return true;
}

static bool nvs_set_current_image_id(const String &id)
{
    _trigger_update_string(DEVCFG_TYPE_CURRENT_IMAGE_ID, id.c_str());
    return true;
}
static bool nvs_get_current_image_id(String &id)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ID].mutex, portMAX_DELAY);
    id = g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ID].current.str_val ? g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ID].current.str_val : "";
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ID].mutex);
    return true;
}

static bool nvs_set_current_image_order(int order)
{
    _trigger_update_int(DEVCFG_TYPE_CURRENT_IMAGE_ORDER, order);
    return true;
}
static bool nvs_get_current_image_order(int &order)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ORDER].mutex, portMAX_DELAY);
    order = g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ORDER].current.int_val;
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ORDER].mutex);
    return true;
}

static bool nvs_set_album_version(const String &version)
{
    _trigger_update_string(DEVCFG_TYPE_ALBUM_VERSION, version.c_str());
    return true;
}
static bool nvs_get_album_version(String &version)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_ALBUM_VERSION].mutex, portMAX_DELAY);
    version = g_deviceCfgs[DEVCFG_TYPE_ALBUM_VERSION].current.str_val ? g_deviceCfgs[DEVCFG_TYPE_ALBUM_VERSION].current.str_val : "";
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_ALBUM_VERSION].mutex);
    return true;
}

static bool nvs_set_sleep_interval(uint32_t interval)
{
    _trigger_update_uint32(DEVCFG_TYPE_SLEEP_INTERVAL, interval);
    return true;
}
static bool nvs_get_sleep_interval(uint32_t &interval)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_SLEEP_INTERVAL].mutex, portMAX_DELAY);
    interval = g_deviceCfgs[DEVCFG_TYPE_SLEEP_INTERVAL].current.uint32_val;
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_SLEEP_INTERVAL].mutex);
    return true;
}

static bool nvs_set_disable_sleep(uint32_t disable)
{
    _trigger_update_uint32(DEVCFG_TYPE_DISABLE_SLEEP, disable);
    return true;
}
static bool nvs_get_disable_sleep(uint32_t &disable)
{
    xSemaphoreTake(g_deviceCfgs[DEVCFG_TYPE_DISABLE_SLEEP].mutex, portMAX_DELAY);
    disable = g_deviceCfgs[DEVCFG_TYPE_DISABLE_SLEEP].current.uint32_val;
    xSemaphoreGive(g_deviceCfgs[DEVCFG_TYPE_DISABLE_SLEEP].mutex);
    return true;
}

static bool nvs_clear_all()
{
    bool success = false;
    if (preferences.begin(NVS_NAMESPACE, false))
    {
        success = preferences.clear();
        preferences.end();
        Log.warningln("NVS cleared. System will restart.");
        delay(1000);
    }
    return success;
}

#define DEFINE_SET_FUNC(name, type, nvs_key, val_suffix)                           \
    static void __set_##name()                                                     \
    {                                                                              \
        DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_##type];                         \
        xSemaphoreTake(cfg.mutex, portMAX_DELAY);                                  \
        if (cfg.last.val_suffix != cfg.current.val_suffix)                         \
        {                                                                          \
            preferences.putBool(nvs_key, cfg.current.val_suffix);                  \
            cfg.last.val_suffix = cfg.current.val_suffix;                          \
            Log.verboseln("NVS Write: %s -> %d", nvs_key, cfg.current.val_suffix); \
        }                                                                          \
        xSemaphoreGive(cfg.mutex);                                                 \
    }

#define DEFINE_SET_FUNC_INT(name, type, nvs_key, val_suffix)                       \
    static void __set_##name()                                                     \
    {                                                                              \
        DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_##type];                         \
        xSemaphoreTake(cfg.mutex, portMAX_DELAY);                                  \
        if (cfg.last.val_suffix != cfg.current.val_suffix)                         \
        {                                                                          \
            preferences.putInt(nvs_key, cfg.current.val_suffix);                   \
            cfg.last.val_suffix = cfg.current.val_suffix;                          \
            Log.verboseln("NVS Write: %s -> %d", nvs_key, cfg.current.val_suffix); \
        }                                                                          \
        xSemaphoreGive(cfg.mutex);                                                 \
    }

#define DEFINE_SET_FUNC_UINT(name, type, nvs_key, val_suffix)                      \
    static bool __set_##name()                                                     \
    {                                                                              \
        bool changed = false;                                                      \
        DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_##type];                         \
        xSemaphoreTake(cfg.mutex, portMAX_DELAY);                                  \
        if (cfg.last.val_suffix != cfg.current.val_suffix)                         \
        {                                                                          \
            size_t written = preferences.putUInt(nvs_key, cfg.current.val_suffix); \
            if (written == sizeof(cfg.current.val_suffix))                         \
            {                                                                      \
                cfg.last.val_suffix = cfg.current.val_suffix;                      \
                changed = true;                                                    \
                Log.verboseln("NVS Write: %s -> %u", nvs_key, cfg.current.val_suffix); \
            }                                                                      \
            else                                                                   \
            {                                                                      \
                Log.errorln("NVS Write failed: %s", nvs_key);                    \
            }                                                                      \
        }                                                                          \
        xSemaphoreGive(cfg.mutex);                                                 \
        return changed;                                                            \
    }

#define DEFINE_SET_FUNC_STR(name, type, nvs_key)                                                                           \
    static void __set_##name()                                                                                             \
    {                                                                                                                      \
        DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_##type];                                                                 \
        xSemaphoreTake(cfg.mutex, portMAX_DELAY);                                                                          \
        bool changed = !((cfg.current.str_val == cfg.last.str_val) ||                                                      \
                         (cfg.current.str_val && cfg.last.str_val && strcmp(cfg.current.str_val, cfg.last.str_val) == 0)); \
        if (changed)                                                                                                       \
        {                                                                                                                  \
            preferences.putString(nvs_key, cfg.current.str_val ? cfg.current.str_val : "");                                \
            if (cfg.last.str_val)                                                                                          \
                free(cfg.last.str_val);                                                                                    \
            cfg.last.str_val = cfg.current.str_val ? strdup(cfg.current.str_val) : nullptr;                                \
            Log.verboseln("NVS Write: %s -> %s", nvs_key, cfg.current.str_val ? cfg.current.str_val : "null");             \
        }                                                                                                                  \
        xSemaphoreGive(cfg.mutex);                                                                                         \
    }

DEFINE_SET_FUNC(device_mode, DEVICE_MODE, NVS_KEY_DEVICE_MODE, bool_val)
DEFINE_SET_FUNC_INT(device_bound, DEVICE_BOUND, NVS_KEY_DEVICE_BOUND, int_val)
DEFINE_SET_FUNC_STR(image_version, IMAGE_VERSION, NVS_KEY_IMAGE_VERSION)
DEFINE_SET_FUNC_INT(image_count, IMAGE_COUNT, NVS_KEY_IMAGE_COUNT, int_val)
DEFINE_SET_FUNC(is_image_in, IS_IMAGE_IN, NVS_KEY_IS_IMG_IN, bool_val)
DEFINE_SET_FUNC_STR(current_image_id, CURRENT_IMAGE_ID, NVS_KEY_CURRENT_IMAGE_ID)
DEFINE_SET_FUNC_INT(current_image_order, CURRENT_IMAGE_ORDER, NVS_KEY_CURRENT_IMAGE_ORDER, int_val)
DEFINE_SET_FUNC_STR(album_version, ALBUM_VERSION, NVS_KEY_ALBUM_VERSION)
DEFINE_SET_FUNC_UINT(sleep_interval, SLEEP_INTERVAL, NVS_KEY_SLEEP_INTERVAL, uint32_val)
DEFINE_SET_FUNC_UINT(disable_sleep, DISABLE_SLEEP, NVS_KEY_DISABLE_SLEEP, uint32_val)

static void __set_wifi_credentials()
{
    Log.verboseln("NVS Write: Updating all WiFi credentials.");
    DeviceCfg &cfg = g_deviceCfgs[DEVCFG_TYPE_WIFI_CREDENTIALS];
    xSemaphoreTake(cfg.mutex, portMAX_DELAY);

    size_t i = 0;
    for (; i < g_wifi_credentials.size(); ++i)
    {
        String key_ssid = String(NVS_KEY_WIFI_SSID_PREFIX) + i;
        String key_pass = String(NVS_KEY_WIFI_PASS_PREFIX) + i;
        preferences.putString(key_ssid.c_str(), g_wifi_credentials[i].first);
        preferences.putString(key_pass.c_str(), g_wifi_credentials[i].second);
    }

    for (; i < MAX_WIFI_CREDENTIALS; ++i)
    {
        String key_ssid = String(NVS_KEY_WIFI_SSID_PREFIX) + i;
        if (preferences.isKey(key_ssid.c_str()))
        {
            String key_pass = String(NVS_KEY_WIFI_PASS_PREFIX) + i;
            preferences.remove(key_ssid.c_str());
            preferences.remove(key_pass.c_str());
        }
        else
        {
            break;
        }
    }

    xSemaphoreGive(cfg.mutex);
}

namespace
{
    struct DeviceInfoTaskState
    {
        EventBits_t bits_devicecfg_mask{0};
        bool timer_running{false};
        bool started{false};
        uint8_t last_sdcard_inserted{0x88};
        uint8_t sdcard_debounce{0x99};
        uint32_t low_battery_check_seconds{0};
        bool low_battery_sleep_pending{false};
        uint32_t low_battery_sleep_started_ms{0};
    };

    static bool is_charging;
    static bool last_is_charging;

    DeviceInfoTaskState g_device_info_state;

    bool block_startup_on_low_battery()
    {
        if (HAL::GetHAL().pmicIsCharging())
        {
            return false;
        }

        float battery_percent = HAL::GetHAL().batteryReadPercent();
        if (battery_percent < 0.0f)
        {
            return false;
        }

        if (battery_percent > LOW_BATTERY_AUTO_SLEEP_PERCENT)
        {
            return false;
        }

        int battery_percent_x10 = static_cast<int>(battery_percent * 10.0f + 0.5f);
        int battery_voltage_mv = static_cast<int>(HAL::GetHAL().batteryReadVoltage() * 1000.0f + 0.5f);
        Log.warningln("[app_device_info] Startup blocked by low battery percent_x10=%d voltage_mv=%d.",
                      battery_percent_x10,
                      battery_voltage_mv);
        esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_LOW_BATTERY, NULL, 0, portMAX_DELAY);
        return true;
    }

    void check_low_battery_auto_sleep(DeviceInfoTaskState &state)
    {
        if (state.low_battery_sleep_pending)
        {
            if (millis() - state.low_battery_sleep_started_ms >= LOW_BATTERY_SLEEP_DELAY_MS && !app_view_is_refreshing())
            {
                EnterDeepSleep();
                state.low_battery_sleep_started_ms = millis();
            }
            return;
        }

        if (app_view_is_refreshing())
        {
            return;
        }

        if (state.low_battery_check_seconds > 0)
        {
            state.low_battery_check_seconds--;
            return;
        }
        state.low_battery_check_seconds = LOW_BATTERY_CHECK_INTERVAL_S;

        if (HAL::GetHAL().pmicIsCharging())
        {
            return;
        }

        float battery_percent = HAL::GetHAL().batteryReadPercent();
        if (battery_percent < 0.0f)
        {
            return;
        }

        if (battery_percent <= LOW_BATTERY_AUTO_SLEEP_PERCENT)
        {
            int battery_percent_x10 = static_cast<int>(battery_percent * 10.0f + 0.5f);
            int battery_voltage_mv = static_cast<int>(HAL::GetHAL().batteryReadVoltage() * 1000.0f + 0.5f);
            Log.warningln("[app_device_info] Low battery percent_x10=%d voltage_mv=%d, showing low battery page.",
                          battery_percent_x10,
                          battery_voltage_mv);
            esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_LOW_BATTERY, NULL, 0, portMAX_DELAY);
            state.low_battery_sleep_pending = true;
            state.low_battery_sleep_started_ms = millis();
        }
    }

    void device_info_process_events(DeviceInfoTaskState &state, TickType_t wait_ticks)
    {
        if (!g_eg_task_wakeup)
        {
            return;
        }

        EventBits_t bits = xEventGroupWaitBits(
            g_eg_task_wakeup,
            EVENT_DEVICECFG_CHANGE | EVENT_TIMER_1S,
            pdTRUE,
            pdFALSE,
            wait_ticks);

        if (bits == 0)
        {
            return;
        }

        preferences.begin(NVS_NAMESPACE, false);

        if (bits & EVENT_DEVICECFG_CHANGE)
        {
            bool iot_state_changed = false;
            EventBits_t bits_devicecfg = xEventGroupWaitBits(
                g_eg_devicecfg_change,
                state.bits_devicecfg_mask,
                pdTRUE,
                pdFALSE,
                0);

            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_DEVICE_MODE))
            {
                __set_device_mode();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_DEVICE_BOUND))
            {
                __set_device_bound();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_IMAGE_VERSION))
            {
                __set_image_version();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_IMAGE_COUNT))
            {
                __set_image_count();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_IS_IMAGE_IN))
            {
                __set_is_image_in();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_CURRENT_IMAGE_ID))
            {
                __set_current_image_id();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_CURRENT_IMAGE_ORDER))
            {
                __set_current_image_order();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_ALBUM_VERSION))
            {
                __set_album_version();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_SLEEP_INTERVAL))
            {
                iot_state_changed |= __set_sleep_interval();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_WIFI_CREDENTIALS))
            {
                __set_wifi_credentials();
            }
            if (bits_devicecfg & EVENT_BIT(DEVCFG_TYPE_DISABLE_SLEEP))
            {
                iot_state_changed |= __set_disable_sleep();
            }

            if (iot_state_changed)
            {
                app_sensecraft_request_iot_report();
            }
        }

        if (bits & EVENT_TIMER_1S)
        {
            if (HAL::GetHAL().sdSupportsHotplugDetection())
            {
                uint8_t sdcard_inserted = static_cast<uint8_t>(HAL::GetHAL().sdIsInserted());
                if (sdcard_inserted == state.sdcard_debounce)
                {
                    if (sdcard_inserted != state.last_sdcard_inserted)
                    {
                        if (app_view_is_refreshing())
                        {
                            Log.infoln("[app_device_info] MicroSD hotplug deferred while display is refreshing.");
                        }
                        else if (sdcard_inserted)
                        {
                            HAL::GetHAL().sdInit();
                            state.last_sdcard_inserted = sdcard_inserted;
                            app_gallery_rebuild_cache_from_manifest();
                            esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_IOT_REPORT, NULL, 0, portMAX_DELAY);
                        }
                        else
                        {
                            HAL::GetHAL().sdDeinit();
                            state.last_sdcard_inserted = sdcard_inserted;
                            app_gallery_rebuild_cache_from_manifest();
                            esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_IOT_REPORT, NULL, 0, portMAX_DELAY);
                        }
                    }
                }
                state.sdcard_debounce = sdcard_inserted;
            }

            is_charging = HAL::GetHAL().pmicIsCharging();
            if (is_charging != last_is_charging)
            {
#if (RETERMINAL_DEEPSLEEP_DISABLE == 0)
                esp_event_post(CTRL_EVENT_BASE, MQTT_EVENT_IOT_REPORT, NULL, 0, portMAX_DELAY);
                last_is_charging = is_charging;
                if (last_is_charging)
                {
                    Log.infoln("[app_device_info] Device is charging");
                    StopDeepSleepTimer();
                }
                else
                {
                    uint32_t disable;
                    nvs_get_disable_sleep(disable);
                    if (!disable)
                    {
                        StartDeepSleepTimer();
                    }
                    Log.warningln("[app_device_info] No input charging");
                }
#endif
            }
        }

        preferences.end();

        if (bits & EVENT_TIMER_1S)
        {
            check_low_battery_auto_sleep(state);
        }
    }

    void device_info_task(void *)
    {
        while (true)
        {
            device_info_process_events(g_device_info_state, portMAX_DELAY);
        }
    }

    bool g_device_info_task_registered = false;
    TaskHandle_t g_device_info_task = nullptr;
}

static const char *bool_text(bool value)
{
    return value ? "true" : "false";
}

static const char *cfg_string(devicecfg_type_t type)
{
    char *value = g_deviceCfgs[type].current.str_val;
    return value ? value : "";
}

static void log_saved_device_config()
{
    Log.infoln("[app_device_info] Saved config:");
    Log.infoln("[app_device_info]   deviceMode=%s", bool_text(g_deviceCfgs[DEVCFG_TYPE_DEVICE_MODE].current.bool_val));
    Log.infoln("[app_device_info]   deviceBound=%d", g_deviceCfgs[DEVCFG_TYPE_DEVICE_BOUND].current.int_val);
    Log.infoln("[app_device_info]   imageVersion=%s", cfg_string(DEVCFG_TYPE_IMAGE_VERSION));
    Log.infoln("[app_device_info]   imageCount=%d", g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].current.int_val);
    Log.infoln("[app_device_info]   isImageIn=%s", bool_text(g_deviceCfgs[DEVCFG_TYPE_IS_IMAGE_IN].current.bool_val));
    Log.infoln("[app_device_info]   currentImageId=%s", cfg_string(DEVCFG_TYPE_CURRENT_IMAGE_ID));
    Log.infoln("[app_device_info]   currentImageOrder=%d", g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ORDER].current.int_val);
    Log.infoln("[app_device_info]   albumVersion=%s", cfg_string(DEVCFG_TYPE_ALBUM_VERSION));
    Log.infoln("[app_device_info]   sleepInterval=%u", g_deviceCfgs[DEVCFG_TYPE_SLEEP_INTERVAL].current.uint32_val);
    Log.infoln("[app_device_info]   deepSleepEnabled=%s", bool_text(g_deviceCfgs[DEVCFG_TYPE_DISABLE_SLEEP].current.uint32_val == 0));
    Log.infoln("[app_device_info]   wifiCredentials=%u", static_cast<unsigned>(g_wifi_credentials.size()));
}

static void play_wakeup_button_tone()
{
    switch (HAL::GetHAL().wakeupReason())
    {
    case WakeupClick::Key0Short:
        hal_indicator_play(HAL_INDICATOR_PRIMARY_ACTION);
        break;

    case WakeupClick::Key1Short:
    case WakeupClick::Key2Short:
        hal_indicator_play(HAL_INDICATOR_CLICK);
        break;

    default:
        break;
    }
}

void DeviceInfoEarlyInit()
{
    if (g_device_info_task_registered)
    {
        Log.warningln("[app_device_info] Device info task already running.");
        return;
    }

    if (g_deviceCfgs == NULL)
    {
        g_deviceCfgs = (DeviceCfg *)ps_calloc(DEVCFG_TYPE_MAX, sizeof(DeviceCfg));
        if (g_deviceCfgs == NULL)
        {
            Log.errorln("✖ NVS Service: Failed to allocate g_deviceCfgs in PSRAM!");
            return;
        }
    }

    g_eg_task_wakeup = xEventGroupCreate();
    g_eg_devicecfg_change = xEventGroupCreate();

    esp_timer_create_args_t timer_args = {.callback = &_timer_cb_debounce};

    for (int i = 0; i < DEVCFG_TYPE_MAX; i++)
    {
        g_deviceCfgs[i].type = (devicecfg_type_t)i;
        g_deviceCfgs[i].mutex = xSemaphoreCreateMutex();
        timer_args.arg = &g_deviceCfgs[i];
        esp_timer_create(&timer_args, &g_deviceCfgs[i].timer_handle);
    }

    timer_args.callback = __timer_cb_every_1s;
    esp_timer_create(&timer_args, &__timer_every_1s);

    timer_args.callback = __wakeup_timer_cb;
    esp_timer_create(&timer_args, &__wakerup_action_timer);

    preferences.begin(NVS_NAMESPACE, false);

    auto safe_strdup = [](const String &s)
    {
        return s.isEmpty() ? nullptr : strdup(s.c_str());
    };

    // deviceMode
    g_deviceCfgs[DEVCFG_TYPE_DEVICE_MODE].current.bool_val = preferences.getBool(NVS_KEY_DEVICE_MODE, false);
    g_deviceCfgs[DEVCFG_TYPE_DEVICE_MODE].last.bool_val = g_deviceCfgs[DEVCFG_TYPE_DEVICE_MODE].current.bool_val;

    // deviceBound
    g_deviceCfgs[DEVCFG_TYPE_DEVICE_BOUND].current.int_val = preferences.getInt(NVS_KEY_DEVICE_BOUND, 0);
    g_deviceCfgs[DEVCFG_TYPE_DEVICE_BOUND].last.int_val = g_deviceCfgs[DEVCFG_TYPE_DEVICE_BOUND].current.int_val;

    // imageVersion
    g_deviceCfgs[DEVCFG_TYPE_IMAGE_VERSION].current.str_val = safe_strdup(preferences.getString(NVS_KEY_IMAGE_VERSION, "0.0"));
    g_deviceCfgs[DEVCFG_TYPE_IMAGE_VERSION].last.str_val = safe_strdup(preferences.getString(NVS_KEY_IMAGE_VERSION, "0.0"));

    // imageCount
    g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].current.int_val = preferences.getInt(NVS_KEY_IMAGE_COUNT, 0);
    g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].last.int_val = g_deviceCfgs[DEVCFG_TYPE_IMAGE_COUNT].current.int_val;

    // isImageIn
    g_deviceCfgs[DEVCFG_TYPE_IS_IMAGE_IN].current.bool_val = preferences.getBool(NVS_KEY_IS_IMG_IN, false);
    g_deviceCfgs[DEVCFG_TYPE_IS_IMAGE_IN].last.bool_val = g_deviceCfgs[DEVCFG_TYPE_IS_IMAGE_IN].current.bool_val;

    // currentImageId
    g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ID].current.str_val = safe_strdup(preferences.getString(NVS_KEY_CURRENT_IMAGE_ID, ""));
    g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ID].last.str_val = safe_strdup(preferences.getString(NVS_KEY_CURRENT_IMAGE_ID, ""));

    // currentImageOrder
    g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ORDER].current.int_val = preferences.getInt(NVS_KEY_CURRENT_IMAGE_ORDER, 0);
    g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ORDER].last.int_val = g_deviceCfgs[DEVCFG_TYPE_CURRENT_IMAGE_ORDER].current.int_val;

    // albumVersion
    g_deviceCfgs[DEVCFG_TYPE_ALBUM_VERSION].current.str_val = safe_strdup(preferences.getString(NVS_KEY_ALBUM_VERSION, "0.0"));
    g_deviceCfgs[DEVCFG_TYPE_ALBUM_VERSION].last.str_val = safe_strdup(preferences.getString(NVS_KEY_ALBUM_VERSION, "0.0"));

    // sleepInterval
    g_deviceCfgs[DEVCFG_TYPE_SLEEP_INTERVAL].current.uint32_val = preferences.getUInt(NVS_KEY_SLEEP_INTERVAL, 60 * 30);
    g_deviceCfgs[DEVCFG_TYPE_SLEEP_INTERVAL].last.uint32_val = g_deviceCfgs[DEVCFG_TYPE_SLEEP_INTERVAL].current.uint32_val;

    // disableSleep
    g_deviceCfgs[DEVCFG_TYPE_DISABLE_SLEEP].current.uint32_val = preferences.getUInt(NVS_KEY_DISABLE_SLEEP, 0);
    g_deviceCfgs[DEVCFG_TYPE_DISABLE_SLEEP].last.uint32_val = g_deviceCfgs[DEVCFG_TYPE_DISABLE_SLEEP].current.uint32_val;

    // WiFi Credentials
    for (int i = 0; i < MAX_WIFI_CREDENTIALS; ++i)
    {
        String key_ssid = String(NVS_KEY_WIFI_SSID_PREFIX) + i;
        String ssid = preferences.getString(key_ssid.c_str(), "");
        if (!ssid.isEmpty())
        {
            String key_pass = String(NVS_KEY_WIFI_PASS_PREFIX) + i;
            String password = preferences.getString(key_pass.c_str(), "");
            g_wifi_credentials.push_back({ssid, password});
        }
        else
        {
            break;
        }
    }

    log_saved_device_config();
    play_wakeup_button_tone();
    preferences.end();
}

bool DeviceInfoInit()
{
    if (block_startup_on_low_battery())
    {
        return false;
    }

    static bool is_need_clear = false;
    bool device_mode;
    nvs_get_device_mode(device_mode);

    RtcDateTime now_dt;
    if (HAL::GetHAL().rtcReadTime(now_dt))
    {
        RtcDateTime today_0430_dt(now_dt.Year(), now_dt.Month(), now_dt.Day(), MAINTENANCE_START_HOUR, MAINTENANCE_START_MINUTE, 0);
        if (now_dt >= today_0430_dt && now_dt < today_0430_dt + MAINTENANCE_WINDOW_SECONDS)
        {
            Log.infoln("Device woke up during maintenance window (04:30:00-04:31)");
            is_need_clear = true;
        }
    }

    switch (HAL::GetHAL().wakeupReason())
    {
    case WakeupClick::None:
        Log.infoln("[Wakeup] Waked up by None");
        is_need_clear = false;
        {
            bool is_image_in = false;
            int image_count = 0;
            nvs_get_is_image_in(is_image_in);
            nvs_get_image_count(image_count);
            if (is_image_in && image_count > 0 && app_gallery_has_playable())
            {
                Log.infoln("[Wakeup] Showing cached local image while background services start.");
                set_local_gallery_session_enabled(device_mode);
                esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, NULL, 1, portMAX_DELAY);
            }
            else if (is_image_in || image_count > 0)
            {
                Log.warningln("[Wakeup] Cached image state is stale, no local image available.");
                SetHasImage(false);
                SetImageCount(0);
            }
        }
        break;

    case WakeupClick::Key0Short:
        Log.infoln("[Wakeup] Waked up by Key0Short");
        is_need_clear = false;
        __wakeup_button = 0;
        app_user_action_mark_pending();
        break;

    case WakeupClick::Touch:
        Log.infoln("[Wakeup] Waked up by Touch");
        is_need_clear = false;
        __wakeup_button = 0;
        app_user_action_mark_pending();
        break;

    case WakeupClick::Key1Short:
        Log.infoln("[Wakeup] Waked up by Key1Short");
        is_need_clear = false;
        set_local_gallery_session_enabled(device_mode);
        __wakeup_button = 1;
        esp_timer_start_once(__wakerup_action_timer, 2000 * 1000);
        app_user_action_mark_refresh_pending(true);

        break;  
    
    case WakeupClick::Key2Short:
        Log.infoln("[Wakeup] Waked up by Key2Short");
        is_need_clear = false;
        set_local_gallery_session_enabled(device_mode);
        __wakeup_button = 2;
        esp_timer_start_once(__wakerup_action_timer, 2000 * 1000);
        app_user_action_mark_refresh_pending(true);

        break;

    case WakeupClick::DoubleLong:
        Log.infoln("[Wakeup] Waked up by DoubleLong");
        is_need_clear = false;

        break;

    case WakeupClick::TripleLong:
        Log.infoln("[Wakeup] Waked up by TripleLong");
        is_need_clear = false;
        set_local_gallery_session_enabled(true);
        esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, NULL, 1, portMAX_DELAY);

        break;

    case WakeupClick::Timer:
        Log.infoln("[Wakeup] Waked up by Timer");
        SetTimerWakeup(true);
        app_gallery_select_next();
        set_local_gallery_session_enabled(device_mode);
        if (device_mode)
        {
            esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, NULL, 1, portMAX_DELAY);
        }

        break;

    default:
        break;
    }

    // clear screen maintenance
    if (is_need_clear)
    {
        // Not connected to network
        set_local_gallery_session_enabled(true);
        esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_CLEAR, NULL, 1, portMAX_DELAY);
    }

    HAL::GetHAL().enableButtonWakeup();

    g_device_info_state.bits_devicecfg_mask = 0;
    for (int i = 0; i < DEVCFG_TYPE_MAX; ++i)
    {
        g_device_info_state.bits_devicecfg_mask |= EVENT_BIT(i);
    }

    if (HAL::GetHAL().sdSupportsHotplugDetection())
    {
        uint8_t sdcard_inserted = static_cast<uint8_t>(HAL::GetHAL().sdIsInserted());
        g_device_info_state.last_sdcard_inserted = sdcard_inserted;
        g_device_info_state.sdcard_debounce = sdcard_inserted;
    }

    BaseType_t task_created = xTaskCreate(
        device_info_task,
        "app_device_info",
        DEVICE_INFO_TASK_STACK_SIZE,
        nullptr,
        DEVICE_INFO_TASK_PRIORITY,
        &g_device_info_task);
    if (task_created != pdPASS)
    {
        g_device_info_task = nullptr;
        Log.errorln("[app_device_info] Failed to create device info task.");
        return true;
    }

    if (__timer_every_1s)
    {
        esp_timer_start_periodic(__timer_every_1s, 1000000);
        g_device_info_state.timer_running = true;
    }

    app_power_manager_set_enabled(GetDeepSleepEnabled());
    app_power_manager_start();

    g_device_info_state.started = true;
    g_device_info_task_registered = true;
    Log.verboseln("✔ NVS Service: Initialized with debounced writer.");
    Log.infoln("app_device info task started");
    return true;
}
