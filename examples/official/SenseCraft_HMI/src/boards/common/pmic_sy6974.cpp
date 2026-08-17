#include "boards/common/pmic_sy6974.h"

#include "ArduinoLog.h"

PmicSy6974::PmicSy6974(TwoWire& wire, Stream& log_stream, int sda_pin, int scl_pin)
    : wire_(wire), chip_(wire, &log_stream), sda_pin_(sda_pin), scl_pin_(scl_pin)
{
}

void PmicSy6974::initBus()
{
    wire_.setPins(sda_pin_, scl_pin_);
    wire_.begin(sda_pin_, scl_pin_);
    wire_.setClock(400000);
}

bool PmicSy6974::init()
{
    detected_ = false;

    bool watchdog_disabled = false;
    for (int i = 0; i < 3; ++i) {
        watchdog_disabled = chip_.disableWatchdog();
        if (watchdog_disabled) {
            break;
        }
        delay(50);
    }
    if (!watchdog_disabled) {
        return false;
    }

    delay(100);
    if (!chip_.setVindpmThreshold(4100)) {
        return false;
    }
    if (!chip_.setInputCurrentLimit(1000)) {
        return false;
    }
    if (!chip_.setChargeCurrent(500)) {
        return false;
    }

    Log.infoln("[Board] PMIC initialization successful");
    detected_ = true;
    return true;
}

bool PmicSy6974::isDetected() const
{
    return detected_;
}

bool PmicSy6974::isCharging()
{
    if (!detected_) {
        return false;
    }

    return chip_.getBusStatus() != "No input";
}

bool PmicSy6974::isBatteryConnected()
{
    if (!detected_) {
        return true;
    }

    uint8_t reg08 = 0;
    uint8_t reg09 = 0;

    chip_.readRegister(SY6974::REG09_NTC, reg09);
    delay(5);
    if (!chip_.readRegister(SY6974::REG09_NTC, reg09)) {
        return false;
    }
    if (!chip_.readRegister(SY6974::REG08_STATUS, reg08)) {
        return false;
    }

    uint8_t ntc_fault = reg09 & 0x07;
    if (ntc_fault == 0x05) {
        return false;
    }

    if (reg09 & 0x08) {
        return false;
    }

    uint8_t bus_stat = (reg08 >> 5) & 0x07;
    uint8_t chrg_stat = (reg08 >> 3) & 0x03;
    uint8_t vsys_stat = reg08 & 0x01;

    if (bus_stat != 0x00 && vsys_stat == 0x01 && chrg_stat == 0x00) {
        return false;
    }

    return true;
}
