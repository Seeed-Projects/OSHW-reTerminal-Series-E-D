#include "APP/app_user_action.h"

#include "APP/app_device_info.h"
#include "APP/app_power_manager.h"
#include "app_config.h"
#include "hal/hal.h"
#include "hal/hal_indicator.h"

#include <atomic>

enum app_user_action_state_t
{
    APP_USER_ACTION_NONE,
    APP_USER_ACTION_PENDING,
    APP_USER_ACTION_REFRESHING,
};

static std::atomic<app_user_action_state_t> g_user_action_state{APP_USER_ACTION_NONE};
static std::atomic<bool> g_refresh_completion_tone{false};

void app_user_action_mark_pending()
{
    MarkUserWakeup();
    g_refresh_completion_tone.store(false);
    g_user_action_state.store(APP_USER_ACTION_PENDING);
}

void app_user_action_mark_refresh_pending(bool completion_tone)
{
    MarkUserWakeup();
    g_refresh_completion_tone.store(completion_tone);
    g_user_action_state.store(APP_USER_ACTION_PENDING);
}

void app_user_action_begin(hal_indicator_pattern_t tone)
{
    app_user_action_mark_pending();
    HAL::GetHAL().ledSet(true);
    hal_indicator_play(tone);
}

void app_user_action_begin_refresh(bool completion_tone)
{
    app_power_manager_acquire(AppPowerOwner::UserAction);
    MarkUserWakeup();
    g_refresh_completion_tone.store(completion_tone);
    g_user_action_state.store(APP_USER_ACTION_REFRESHING);
    HAL::GetHAL().ledSet(true);
    hal_indicator_play(HAL_INDICATOR_CLICK);
}

void app_user_action_finish_refresh()
{
    app_user_action_state_t previous_state = g_user_action_state.exchange(APP_USER_ACTION_NONE);
    if (previous_state == APP_USER_ACTION_NONE)
    {
        return;
    }

    if (previous_state == APP_USER_ACTION_REFRESHING)
    {
        app_power_manager_release(AppPowerOwner::UserAction);
    }
    HAL::GetHAL().ledSet(false);
    if (g_refresh_completion_tone.exchange(false))
    {
        hal_indicator_play(HAL_INDICATOR_REFRESH_DONE);
    }
}

bool app_user_action_is_active()
{
    return g_user_action_state.load() == APP_USER_ACTION_REFRESHING;
}
