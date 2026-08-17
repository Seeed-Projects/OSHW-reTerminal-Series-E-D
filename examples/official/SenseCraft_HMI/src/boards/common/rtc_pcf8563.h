#pragma once

#include <Arduino.h>
#include <RtcPCF8563.h>
#include <Wire.h>

class RtcPcf8563 {
public:
    explicit RtcPcf8563(TwoWire& wire);

    bool init();
    bool isAvailable() const { return initialized_; }
    bool readTime(RtcDateTime& dt);
    bool setTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);

private:
    bool hasError(const char* topic);

    RtcPCF8563<TwoWire> rtc_;
    bool initialized_ = false;
};
