#include <Arduino.h>
#include "hal/hal.h"
#include "hal/hal_indicator.h"
#include "APP/app_sensecraft.h"
#include "APP/app_view.h"
#include "APP/app_download.h"
#include "APP/app_gallery.h"
#include "APP/app_input.h"
#include "APP/app_action.h"
#include "APP/app_device_info.h"
#include "APP/app_power_manager.h"
#include "APP/app_wifi.h"
#include "APP/app_ble.h"
#include "APP/app_atcmd.h"
#include "utils/print_mem.h"
#include "minirt.h"

void setup()
{
  esp_event_loop_create_default();

  HAL::GetHAL().init();
  app_power_manager_init();
  hal_indicator_init();

  DeviceInfoEarlyInit();
  app_gallery_init();
  app_view_init();
  if (!DeviceInfoInit()) {
    uint32_t low_battery_block_start = millis();
    while (true) {
      MiniRT::run();
      if (millis() - low_battery_block_start > 15000 && !app_view_is_refreshing()) {
        EnterDeepSleep();
        low_battery_block_start = millis();
      }
      delay(1);
    }
  }
  app_input_init();
  app_wifi_init();
  app_action_init();
  app_download_init();
  app_task_init();
  app_ble_init();
  app_atcmd_init();
}


void loop()
{
  MiniRT::run();

#if RETERMINAL_DEBUG
  static uint32_t last_mem_log = 0;
  const uint32_t now = millis();
  if (now - last_mem_log > 10000) {
    last_mem_log = now;
    printMemoryStatus();
  }
#endif

  delay(1);
}
