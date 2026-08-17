#pragma once

#include <Arduino.h>
#include <Wire.h>

class SY6974
{
public:
    static constexpr uint8_t I2C_ADDR = 0x6B;
    static constexpr uint8_t REG00_IINLIM = 0x00;
    static constexpr uint8_t REG02_ICHG = 0x02;
    static constexpr uint8_t REG05_WATCHDOG = 0x05;
    static constexpr uint8_t REG06_VINDPM = 0x06;
    static constexpr uint8_t REG08_STATUS = 0x08;
    static constexpr uint8_t REG09_NTC = 0x09;
    static constexpr uint8_t REG0A_BUS_GD = 0x0A;

    SY6974(TwoWire &wire, Stream *logStream = nullptr);

    void setLogStream(Stream *logStream);

    bool disableWatchdog();
    bool setVindpmThreshold(int threshold_mV);
    bool setInputCurrentLimit(int current_mA);
    bool setChargeCurrent(int current_mA);

    int getInputCurrentLimit();
    int getChargeCurrentLimit();
    String getChargeStatus();
    String getBusStatus();
    bool isUsbAttached();

    bool readRegister(uint8_t regAddr, uint8_t &data);
    bool writeRegister(uint8_t regAddr, uint8_t data);

private:
    TwoWire &wire_;
    Stream *log_;
};
