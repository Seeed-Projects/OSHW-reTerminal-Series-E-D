#include "boards/common/gt911_touch.h"

#include "ArduinoLog.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"

Gt911Touch::Gt911Touch(TwoWire& wire, int int_pin, int rst_pin)
    : wire_(wire), int_pin_(int_pin), rst_pin_(rst_pin)
{
}

namespace
{
bool probe_gt911(TwoWire& wire, int int_pin, int rst_pin)
{
    pinMode(int_pin, OUTPUT);
    pinMode(rst_pin, OUTPUT);

    digitalWrite(rst_pin, LOW);
    digitalWrite(int_pin, LOW);
    delay(12);

    digitalWrite(rst_pin, HIGH);
    delay(8);

    pinMode(int_pin, INPUT);
    delay(50);

    wire.beginTransmission(GT911_ADDR1);
    return wire.endTransmission() == 0;
}
}

void Gt911Touch::init(uint16_t width, uint16_t height)
{
    initialized_ = false;
    if (!probe_gt911(wire_, int_pin_, rst_pin_))
    {
        Log.warningln("[Board] GT911 touch controller not found; touch input disabled.");
        return;
    }

    touch_.begin(int_pin_, rst_pin_, width, height, wire_);

    GTGestureConfig cfg;
    cfg.swipe_min_distance = 100;
    cfg.swipe_max_duration_ms = 700;
    cfg.tap_max_duration_ms = 220;
    cfg.tap_max_movement = 25;
    cfg.double_tap_max_interval_ms = 350;
    cfg.double_tap_max_distance = 45;
    touch_.setGestureConfig(cfg);

    initialized_ = true;
    Log.infoln("[Board] GT911 Module Initialize: Succeeded");
    Log.infoln("[Board] Up to 5 contact points can be used with this driver");
}

void Gt911Touch::loop()
{
    if (initialized_) {
        touch_.loop();
    }
}

void Gt911Touch::onGesture(GestureCallback callback)
{
    touch_.onGesture(callback);
}

void Gt911Touch::enableGestureWakeup(int active_level, uint32_t idle_timeout_ms)
{
    if (!initialized_) {
        return;
    }

    touch_.clearGesture();
    touch_.enterGestureMode();

    const gpio_num_t wake_pin = static_cast<gpio_num_t>(int_pin_);
    rtc_gpio_init(wake_pin);
    rtc_gpio_set_direction(wake_pin, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis(wake_pin);
    rtc_gpio_pulldown_en(wake_pin);

    const int idle_level = active_level ? 0 : 1;
    uint32_t wait_start = millis();
    while (rtc_gpio_get_level(wake_pin) != idle_level && (millis() - wait_start) < idle_timeout_ms) {
        delay(5);
    }

    int level_before_sleep = rtc_gpio_get_level(wake_pin);
    Log.verboseln("[Board] Touch wake pin level before sleep: %d", level_before_sleep);
    if (level_before_sleep != idle_level) {
        Log.warningln("[Board] Touch wake pin still active, deep sleep may wake immediately.");
    }

    esp_sleep_enable_ext0_wakeup(wake_pin, active_level);
}
