#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "GT911.h"

class Gt911Touch {
public:
    Gt911Touch(TwoWire& wire, int int_pin, int rst_pin);

    void init(uint16_t width, uint16_t height);
    bool isAvailable() const { return initialized_; }
    void loop();
    void onGesture(GestureCallback callback);
    void enableGestureWakeup(int active_level, uint32_t idle_timeout_ms);

private:
    TwoWire& wire_;
    GT911 touch_;
    int int_pin_;
    int rst_pin_;
    bool initialized_ = false;
};
