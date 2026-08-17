#include "APP/app_action.h"

#include "APP/app_device_info.h"
#include "APP/app_gallery.h"
#include "APP/app_input.h"
#include "APP/app_power_manager.h"
#include "APP/app_sensecraft.h"
#include "APP/app_user_action.h"
#include "APP/app_view.h"
#include "APP/app_wifi.h"

#include <WiFi.h>
#include "ArduinoLog.h"

static bool app_action_is_busy()
{
    return app_view_is_refreshing() || app_wifi_is_provisioning() || app_user_action_is_active();
}

static void request_cloud_refresh(hal_indicator_pattern_t tone = HAL_INDICATOR_CLICK)
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    if (app_action_is_busy())
    {
        Log.warningln("[app_action] Cloud refresh ignored while device is busy.");
        return;
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        Log.warningln("[app_action] Cloud refresh ignored because WiFi is not connected.");
        return;
    }

    const char *target_id = nullptr;
    app_gallery_image_ref_t image = {};
    if (IsGalleryContent() && app_gallery_current(&image))
    {
        target_id = image.id;
        Log.infoln("[app_action] Requesting current image refresh: id=%s order=%d",
                   image.id,
                   image.order);
    }

    request_image_resource(target_id);
    app_user_action_begin(tone);
}

static void handle_primary_action()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    const bool busy = app_action_is_busy();

    int image_count = GetImageCount();
    const wl_status_t wifi_status = WiFi.status();
    Log.infoln("[app_action] Primary action: wifi_status=%d, image_count=%d",
               static_cast<int>(wifi_status),
               image_count);

    if (wifi_status != WL_CONNECTED)
    {
        if (busy)
        {
            Log.warningln("[app_action] Primary action ignored while device is busy.");
            return;
        }

        Log.infoln("[app_action] Requesting manual WiFi autoconnect.");
        app_user_action_begin(HAL_INDICATOR_PRIMARY_ACTION);
        app_wifi_request_autoconnect(true);
        return;
    }

    if (image_count > 0)
    {
        if (busy)
        {
            Log.warningln("[app_action] Primary action ignored while device is busy.");
            return;
        }

        Log.infoln("[app_action] WiFi connected; requesting image resource refresh.");
        request_cloud_refresh(HAL_INDICATOR_PRIMARY_ACTION);
    }
}

static void app_action_input_event_handler(void *, esp_event_base_t, int32_t event_id, void *)
{
    switch (event_id)
    {
    case APP_INPUT_EVENT_PRIMARY_ACTION:
        handle_primary_action();
        break;

    case APP_INPUT_EVENT_REQUEST_CLOUD_REFRESH:
        request_cloud_refresh();
        break;

    default:
        break;
    }
}

void app_action_init()
{
    esp_event_handler_register(APP_INPUT_EVENT_BASE, APP_INPUT_EVENT_PRIMARY_ACTION, &app_action_input_event_handler, NULL);
    esp_event_handler_register(APP_INPUT_EVENT_BASE, APP_INPUT_EVENT_REQUEST_CLOUD_REFRESH, &app_action_input_event_handler, NULL);
}
