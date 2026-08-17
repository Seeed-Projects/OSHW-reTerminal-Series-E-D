#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "SY6974.h"

class PmicSy6974 {
public:
    PmicSy6974(TwoWire& wire, Stream& log_stream, int sda_pin, int scl_pin);

    void initBus();
    bool init();
    bool isDetected() const;
    bool isCharging();
    bool isBatteryConnected();

private:
    TwoWire& wire_;
    SY6974 chip_;
    int sda_pin_;
    int scl_pin_;
    bool detected_ = false;
};
