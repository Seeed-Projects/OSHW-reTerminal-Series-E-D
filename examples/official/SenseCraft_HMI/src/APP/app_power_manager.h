#pragma once

#include <Arduino.h>
#include <stdint.h>

static constexpr uint64_t APP_POWER_ACTIVE_WINDOW_US = 1ULL * 60ULL * 1000ULL * 1000ULL;

enum class AppPowerOwner : uint8_t
{
    Display = 0,
    Download,
    Provisioning,
    UserAction,
    Config,
    CloudWait,
    WifiConnect,
    Max,
};

void app_power_manager_init();
void app_power_manager_start();

void app_power_manager_activity(AppPowerOwner owner);
void app_power_manager_acquire(AppPowerOwner owner);
void app_power_manager_release(AppPowerOwner owner);

void app_power_manager_set_enabled(bool enabled);
void app_power_manager_schedule_sleep_check(uint64_t delay_us);
void app_power_manager_stop_sleep_check();

bool app_power_manager_has_blocker();
uint32_t app_power_manager_owner_mask();
const char *app_power_manager_owner_name(AppPowerOwner owner);

void app_power_manager_enter_sleep_now();

class AppPowerGuard
{
public:
    explicit AppPowerGuard(AppPowerOwner owner);
    ~AppPowerGuard();

    AppPowerGuard(const AppPowerGuard &) = delete;
    AppPowerGuard &operator=(const AppPowerGuard &) = delete;

private:
    AppPowerOwner owner_;
};
