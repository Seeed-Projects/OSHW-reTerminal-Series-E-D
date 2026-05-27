/*
 * LowPower_DeepSleep — ESP32-S3 Deep Sleep Demo
 *
 * Compatible with : reTerminal E1001 / E1002 / E1003 / E1004
 *
 * Behaviour
 * ---------
 * 1. Power on / wakeup → print status → wait SLEEP_DELAY_SEC seconds.
 * 2. Enter deep sleep (~14 µA).
 * 3. Press the button → chip wakes, go to step 1.
 *
 * How to verify deep sleep is actually working
 * --------------------------------------------
 * loop() contains a print statement that should NEVER execute.
 * If deep sleep fails and the CPU keeps running, loop() will run
 * and you will see "[ERROR] deep sleep did not start!" in the serial
 * monitor.  Silence after "[SLEEP]" means the device is truly asleep.
 *
 * How to use
 * ----------
 * 1. Uncomment the ONE device line in USER CONFIGURATION.
 * 2. Flash (board: XIAO_ESP32S3, PSRAM: OPI PSRAM, Flash: 8 MB).
 * 3. Open serial monitor on the carrier USB-UART bridge
 *    (GPIO43 TX / GPIO44 RX, 115200 baud — Serial1, not USB-CDC).
 *
 * Required libraries
 * ------------------
 * All built-in — no Library Manager installs needed:
 *   esp_sleep.h     (ESP32 deep sleep API)
 *   driver/rtc_io.h (keep-alive domain GPIO pull-up, needed during deep sleep)
 */

// ============================================================
// USER CONFIGURATION
// ============================================================

// How many seconds to stay awake before entering deep sleep.
#define SLEEP_DELAY_SEC   5

// --- Wake-up button pin ---
// Uncomment the ONE line that matches your device.
// Only GPIO0–GPIO21 can wake the ESP32-S3 from deep sleep.
//
#define PIN_WAKE_BTN   3   // E1001 / E1002 / E1003 — KEY0
// #define PIN_WAKE_BTN   4   // E1004               — KEY0

// ============================================================
// END OF USER CONFIGURATION
// ============================================================

#include "esp_sleep.h"
#include "driver/rtc_io.h"

#define PIN_SERIAL_RX   44
#define PIN_SERIAL_TX   43
#define LOG             Serial1

// Survives deep sleep — increments on every wakeup.
RTC_DATA_ATTR static int s_bootCount = 0;

static const char* wakeupReason()
{
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_EXT1:  return "GPIO button (EXT1)";
        default:                     return "power-on / manual reset";
    }
}

void setup()
{
    s_bootCount++;

    LOG.begin(115200, SERIAL_8N1, PIN_SERIAL_RX, PIN_SERIAL_TX);
    delay(100);

    LOG.println("========================================");
    LOG.println("  LowPower_DeepSleep — reTerminal E");
    LOG.println("========================================");
    LOG.printf("[WAKE] Boot #%d — wakeup: %s\n", s_bootCount, wakeupReason());
    LOG.printf("[WAKE] Entering deep sleep in %d seconds...\n", SLEEP_DELAY_SEC);
    LOG.printf("[WAKE] Press GPIO%d button to wake up.\n", PIN_WAKE_BTN);

    delay((uint32_t)SLEEP_DELAY_SEC * 1000);

    esp_sleep_enable_ext1_wakeup(1ULL << PIN_WAKE_BTN, ESP_EXT1_WAKEUP_ANY_LOW);

    // Normal GPIO pull-up is off during deep sleep; use keep-alive domain instead.
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(PIN_WAKE_BTN));
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(PIN_WAKE_BTN));

    LOG.println("[SLEEP] Entering deep sleep now.");
    LOG.flush();
    delay(10);

    esp_deep_sleep_start();
}

void loop()
{
    // esp_deep_sleep_start() in setup() never returns, so loop() is never reached.
    // If you see this message, deep sleep failed to start.
    LOG.println("[ERROR] deep sleep did not start!");
    delay(1000);
}
