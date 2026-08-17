#include "boards/common/sht40_sensor.h"

#include "ArduinoLog.h"

Sht40Sensor::Sht40Sensor(TwoWire& wire)
    : wire_(wire)
{
}

bool Sht40Sensor::init()
{
    initialized_ = false;
    initialized_ = sensor_.begin(&wire_);
    if (!initialized_) {
        delay(100);
        initialized_ = sensor_.begin(&wire_);
    }

    if (!initialized_) {
        Log.errorln("[Board] Temperature and humidity sensor initialization failed");
        return false;
    }

    Log.infoln("[Board] Temperature and humidity sensor initialization succeed");
    return true;
}

SHT40_DATA Sht40Sensor::read()
{
    SHT40_DATA data = {0.0f, 0.0f};
    if (!initialized_) {
        return data;
    }

    sensors_event_t humidity;
    sensors_event_t temp;
    sensor_.getEvent(&humidity, &temp);

    data.temperature = temp.temperature;
    data.humidity = humidity.relative_humidity;
    return data;
}
