#pragma once

#include <Arduino.h>
#include <Adafruit_SHT4x.h>
#include <Wire.h>

typedef struct {
    float temperature;
    float humidity;
} SHT40_DATA;

class Sht40Sensor {
public:
    explicit Sht40Sensor(TwoWire& wire);

    bool init();
    bool isAvailable() const { return initialized_; }
    SHT40_DATA read();

private:
    TwoWire& wire_;
    Adafruit_SHT4x sensor_;
    bool initialized_ = false;
};
