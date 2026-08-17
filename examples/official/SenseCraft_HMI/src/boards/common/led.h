#pragma once

#include <Arduino.h>

class Led {
public:
    Led(int pin, uint8_t on_level);

    void init();
    void set(bool on);

private:
    int pin_;
    uint8_t on_level_;
};
