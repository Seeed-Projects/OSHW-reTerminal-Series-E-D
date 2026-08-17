#include "APP/app_power_manager.h"

#include "APP/app_device_info.h"

#include <ArduinoLog.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <limits.h>

namespace
{
    static constexpr size_t kOwnerCount = static_cast<size_t>(AppPowerOwner::Max);

    SemaphoreHandle_t g_power_mutex = nullptr;
    esp_timer_handle_t g_sleep_timer = nullptr;
    uint32_t g_owner_ref_counts[kOwnerCount] = {};
    uint32_t g_total_ref_count = 0;
    bool g_initialized = false;
    bool g_started = false;
    bool g_enabled = true;
    bool g_sleep_timer_running = false;
    bool g_has_pending_sleep_delay = false;
    uint64_t g_pending_sleep_delay_us = APP_POWER_ACTIVE_WINDOW_US;

    bool owner_is_valid(AppPowerOwner owner)
    {
        return static_cast<size_t>(owner) < kOwnerCount;
    }

    uint32_t owner_mask_locked()
    {
        uint32_t mask = 0;
        for (size_t i = 0; i < kOwnerCount; ++i)
        {
            if (g_owner_ref_counts[i] > 0)
            {
                mask |= (1UL << i);
            }
        }
        return mask;
    }

    void stop_sleep_timer_locked()
    {
        if (!g_sleep_timer || !g_sleep_timer_running)
        {
            return;
        }

        esp_err_t err = esp_timer_stop(g_sleep_timer);
        if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        {
            Log.warningln("[app_power] Failed to stop sleep timer: err=%d", err);
        }
        g_sleep_timer_running = false;
    }

    void clear_pending_sleep_delay_locked()
    {
        g_has_pending_sleep_delay = false;
        g_pending_sleep_delay_us = APP_POWER_ACTIVE_WINDOW_US;
    }

    uint64_t take_sleep_delay_locked()
    {
        const uint64_t delay_us = g_has_pending_sleep_delay
            ? g_pending_sleep_delay_us
            : APP_POWER_ACTIVE_WINDOW_US;
        clear_pending_sleep_delay_locked();
        return delay_us;
    }

    void schedule_sleep_check_locked(uint64_t delay_us)
    {
        if (!g_started || !g_enabled || !g_sleep_timer)
        {
            return;
        }

        if (g_total_ref_count > 0)
        {
            stop_sleep_timer_locked();
            Log.verboseln("[app_power] Sleep check deferred, owner_mask=0x%08x", owner_mask_locked());
            return;
        }

        if (g_sleep_timer_running)
        {
            stop_sleep_timer_locked();
        }

        esp_err_t err = esp_timer_start_once(g_sleep_timer, delay_us);
        if (err == ESP_OK)
        {
            g_sleep_timer_running = true;
            Log.verboseln("[app_power] Sleep check scheduled in %llu us", delay_us);
        }
        else
        {
            Log.warningln("[app_power] Failed to schedule sleep check: err=%d", err);
        }
    }

    void sleep_timer_callback(void *)
    {
        bool should_enter_sleep = false;
        uint32_t owner_mask = 0;

        if (g_power_mutex && xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
        {
            g_sleep_timer_running = false;
            owner_mask = owner_mask_locked();
            should_enter_sleep = g_enabled && g_total_ref_count == 0;
            xSemaphoreGive(g_power_mutex);
        }

        if (!should_enter_sleep)
        {
            if (owner_mask != 0)
            {
                Log.verboseln("[app_power] Sleep check ignored, owner_mask=0x%08x", owner_mask);
            }
            return;
        }

        if (!GetDeepSleepEnabled())
        {
            Log.verboseln("[app_power] Sleep check ignored because deep sleep is disabled.");
            return;
        }

        Log.verboseln("[app_power] Active window expired, entering sleep decision.");
        app_power_manager_enter_sleep_now();
    }
}

const char *app_power_manager_owner_name(AppPowerOwner owner)
{
    switch (owner)
    {
    case AppPowerOwner::Display:
        return "display";
    case AppPowerOwner::Download:
        return "download";
    case AppPowerOwner::Provisioning:
        return "provisioning";
    case AppPowerOwner::UserAction:
        return "user_action";
    case AppPowerOwner::Config:
        return "config";
    case AppPowerOwner::CloudWait:
        return "cloud_wait";
    case AppPowerOwner::WifiConnect:
        return "wifi_connect";
    case AppPowerOwner::Max:
        return "none";
    }
    return "invalid";
}

void app_power_manager_init()
{
    if (g_initialized)
    {
        return;
    }

    g_power_mutex = xSemaphoreCreateMutex();
    if (!g_power_mutex)
    {
        Log.errorln("[app_power] Failed to create power manager mutex.");
        return;
    }

    esp_timer_create_args_t timer_args = {
        .callback = sleep_timer_callback,
    };

    esp_err_t err = esp_timer_create(&timer_args, &g_sleep_timer);
    if (err != ESP_OK)
    {
        Log.errorln("[app_power] Failed to create sleep timer: err=%d", err);
        return;
    }

    g_initialized = true;
    Log.infoln("[app_power] Power manager initialized.");
}

void app_power_manager_start()
{
    if (!g_initialized)
    {
        app_power_manager_init();
    }
    if (!g_initialized)
    {
        return;
    }

    if (xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
    {
        g_started = true;
        clear_pending_sleep_delay_locked();
        schedule_sleep_check_locked(APP_POWER_ACTIVE_WINDOW_US);
        xSemaphoreGive(g_power_mutex);
    }
}

void app_power_manager_activity(AppPowerOwner owner)
{
    if (!owner_is_valid(owner))
    {
        Log.warningln("[app_power] Ignored activity from invalid owner=%d", static_cast<int>(owner));
        return;
    }

    if (!g_initialized)
    {
        return;
    }

    if (xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
    {
        Log.verboseln("[app_power] Activity owner=%s", app_power_manager_owner_name(owner));
        clear_pending_sleep_delay_locked();
        schedule_sleep_check_locked(APP_POWER_ACTIVE_WINDOW_US);
        xSemaphoreGive(g_power_mutex);
    }
}

void app_power_manager_acquire(AppPowerOwner owner)
{
    if (!owner_is_valid(owner))
    {
        Log.warningln("[app_power] Ignored acquire from invalid owner=%d", static_cast<int>(owner));
        return;
    }

    if (!g_initialized)
    {
        return;
    }

    if (xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
    {
        const size_t index = static_cast<size_t>(owner);
        if (g_owner_ref_counts[index] == UINT32_MAX || g_total_ref_count == UINT32_MAX)
        {
            Log.warningln("[app_power] Ignored acquire overflow owner=%s", app_power_manager_owner_name(owner));
            xSemaphoreGive(g_power_mutex);
            return;
        }

        if (g_total_ref_count == 0)
        {
            stop_sleep_timer_locked();
        }

        ++g_owner_ref_counts[index];
        ++g_total_ref_count;
        Log.infoln("[app_power] acquire owner=%s owner_ref=%u total_ref=%u owner_mask=0x%08x",
                   app_power_manager_owner_name(owner),
                   static_cast<unsigned>(g_owner_ref_counts[index]),
                   static_cast<unsigned>(g_total_ref_count),
                   owner_mask_locked());
        xSemaphoreGive(g_power_mutex);
    }
}

void app_power_manager_release(AppPowerOwner owner)
{
    if (!owner_is_valid(owner))
    {
        Log.warningln("[app_power] Ignored release from invalid owner=%d", static_cast<int>(owner));
        return;
    }

    if (!g_initialized)
    {
        return;
    }

    if (xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
    {
        const size_t index = static_cast<size_t>(owner);
        if (g_owner_ref_counts[index] == 0)
        {
            Log.warningln("[app_power] release without acquire owner=%s total_ref=%u owner_mask=0x%08x",
                          app_power_manager_owner_name(owner),
                          static_cast<unsigned>(g_total_ref_count),
                          owner_mask_locked());
            xSemaphoreGive(g_power_mutex);
            return;
        }

        --g_owner_ref_counts[index];
        --g_total_ref_count;

        Log.infoln("[app_power] release owner=%s owner_ref=%u total_ref=%u owner_mask=0x%08x",
                   app_power_manager_owner_name(owner),
                   static_cast<unsigned>(g_owner_ref_counts[index]),
                   static_cast<unsigned>(g_total_ref_count),
                   owner_mask_locked());

        if (g_total_ref_count == 0)
        {
            schedule_sleep_check_locked(take_sleep_delay_locked());
        }
        xSemaphoreGive(g_power_mutex);
    }
}

void app_power_manager_set_enabled(bool enabled)
{
    if (!g_initialized)
    {
        return;
    }

    if (xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
    {
        g_enabled = enabled;
        if (!enabled)
        {
            clear_pending_sleep_delay_locked();
            stop_sleep_timer_locked();
        }
        else
        {
            clear_pending_sleep_delay_locked();
            schedule_sleep_check_locked(APP_POWER_ACTIVE_WINDOW_US);
        }
        xSemaphoreGive(g_power_mutex);
    }
}

void app_power_manager_schedule_sleep_check(uint64_t delay_us)
{
    if (!g_initialized)
    {
        return;
    }

    if (xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
    {
        g_has_pending_sleep_delay = true;
        g_pending_sleep_delay_us = delay_us;
        schedule_sleep_check_locked(delay_us);
        xSemaphoreGive(g_power_mutex);
    }
}

void app_power_manager_stop_sleep_check()
{
    if (!g_initialized)
    {
        return;
    }

    if (xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
    {
        clear_pending_sleep_delay_locked();
        stop_sleep_timer_locked();
        xSemaphoreGive(g_power_mutex);
    }
}

bool app_power_manager_has_blocker()
{
    if (!g_initialized)
    {
        return false;
    }

    bool blocked = false;
    if (xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
    {
        blocked = g_total_ref_count > 0;
        xSemaphoreGive(g_power_mutex);
    }
    return blocked;
}

uint32_t app_power_manager_owner_mask()
{
    if (!g_initialized)
    {
        return 0;
    }

    uint32_t mask = 0;
    if (xSemaphoreTake(g_power_mutex, portMAX_DELAY) == pdTRUE)
    {
        mask = owner_mask_locked();
        xSemaphoreGive(g_power_mutex);
    }
    return mask;
}

void app_power_manager_enter_sleep_now()
{
    EnterDeepSleep();
}

AppPowerGuard::AppPowerGuard(AppPowerOwner owner)
    : owner_(owner)
{
    app_power_manager_acquire(owner_);
}

AppPowerGuard::~AppPowerGuard()
{
    app_power_manager_release(owner_);
}
