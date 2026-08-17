#pragma once

#include <Arduino.h>

class Buzzer {
public:
    explicit Buzzer(int pin = -1);

    bool init();
    bool isAvailable() const { return initialized_; }
    bool beep(uint32_t frequency_hz, uint32_t duration_ms);
    void stop();

private:
    int pin_;
    bool initialized_ = false;
};
