#include "SY6974.h"

SY6974::SY6974(TwoWire &wire, Stream *logStream)
    : wire_(wire), log_(logStream)
{
}

void SY6974::setLogStream(Stream *logStream)
{
    log_ = logStream;
}

bool SY6974::writeRegister(uint8_t regAddr, uint8_t data)
{
    wire_.beginTransmission(I2C_ADDR);
    wire_.write(regAddr);
    wire_.write(data);
    return wire_.endTransmission() == 0;
}

bool SY6974::readRegister(uint8_t regAddr, uint8_t &data)
{
    wire_.beginTransmission(I2C_ADDR);
    wire_.write(regAddr);
    if (wire_.endTransmission(false) != 0)
    {
        return false;
    }

    if (wire_.requestFrom(I2C_ADDR, static_cast<uint8_t>(1)) == 1)
    {
        data = wire_.read();
        return true;
    }

    return false;
}

bool SY6974::disableWatchdog()
{
    uint8_t reg_data;

    if (!readRegister(REG05_WATCHDOG, reg_data))
    {
        if (log_)
        {
            log_->println("DisableWatchdog Error: Failed to read REG05");
        }
        return false;
    }

    reg_data = reg_data & 0xCF;

    if (writeRegister(REG05_WATCHDOG, reg_data))
    {
        if (log_)
        {
            log_->println("Watchdog Timer Disabled. Host Mode is persistent.");
        }
        return true;
    }

    if (log_)
    {
        log_->println("DisableWatchdog Error: Failed to write REG05");
    }
    return false;
}

bool SY6974::setVindpmThreshold(int threshold_mV)
{
    if (threshold_mV < 3900)
        threshold_mV = 3900;
    if (threshold_mV > 5400)
        threshold_mV = 5400;

    uint8_t vindpm_val = static_cast<uint8_t>((threshold_mV - 3900) / 100);

    uint8_t reg_data;
    if (!readRegister(REG06_VINDPM, reg_data))
    {
        if (log_)
        {
            log_->println("SetVindpmThreshold Error: Failed to read REG06");
        }
        return false;
    }

    reg_data = (reg_data & 0xF0) | (vindpm_val & 0x0F);

    if (writeRegister(REG06_VINDPM, reg_data))
    {
        if (log_)
        {
            log_->print("VINDPM Threshold Set to: ");
            log_->print(3900 + (vindpm_val * 100));
            log_->println(" mV");
        }
        return true;
    }
    if (log_)
    {
        log_->println("SetVindpmThreshold Error: Failed to write REG06");
    }
    return false;
}

bool SY6974::setInputCurrentLimit(int current_mA)
{
    if (current_mA < 100)
        current_mA = 100;
    if (current_mA > 3200)
        current_mA = 3200;

    uint8_t iinlim_val = static_cast<uint8_t>((current_mA - 100) / 100);
    uint8_t reg_data;
    if (!readRegister(REG00_IINLIM, reg_data))
    {
        if (log_)
        {
            log_->println("SetInputLimit Error: Failed to read REG00");
        }
        return false;
    }

    reg_data = (reg_data & 0xE0) | (iinlim_val & 0x1F);
    if (writeRegister(REG00_IINLIM, reg_data))
    {
        if (log_)
        {
            log_->print("Input Limit Set to: ");
            log_->print(100 + (iinlim_val * 100));
            log_->println(" mA");
        }
        return true;
    }
    if (log_)
    {
        log_->println("SetInputLimit Error: Failed to write REG00");
    }
    return false;
}

bool SY6974::setChargeCurrent(int current_mA)
{
    if (current_mA < 0)
        current_mA = 0;
    if (current_mA > 3000)
        current_mA = 3000;

    uint8_t ichg_val = static_cast<uint8_t>(current_mA / 60);
    uint8_t reg_data;
    if (!readRegister(REG02_ICHG, reg_data))
    {
        if (log_)
        {
            log_->println("SetChargeCurrent Error: Failed to read REG02");
        }
        return false;
    }
    reg_data = (reg_data & 0xC0) | (ichg_val & 0x3F);
    if (writeRegister(REG02_ICHG, reg_data))
    {
        if (log_)
        {
            log_->print("Charge Current Set to: ");
            log_->print(ichg_val * 60);
            log_->println(" mA");
        }
        return true;
    }
    if (log_)
    {
        log_->println("SetChargeCurrent Error: Failed to write REG02");
    }
    return false;
}

int SY6974::getInputCurrentLimit()
{
    uint8_t reg_data;
    if (readRegister(REG00_IINLIM, reg_data))
    {
        uint8_t iinlim_val = reg_data & 0x1F;
        int current_mA = 100 + (static_cast<int>(iinlim_val) * 100);
        return current_mA;
    }

    if (log_)
    {
        log_->println("GetInputCurrentLimit Error: Failed to read REG00");
    }
    return -1;
}

int SY6974::getChargeCurrentLimit()
{
    uint8_t reg_data;
    if (readRegister(REG02_ICHG, reg_data))
    {
        uint8_t ichg_val = reg_data & 0x3F;
        int current_mA = static_cast<int>(ichg_val) * 60;
        return current_mA;
    }

    if (log_)
    {
        log_->println("GetChargeCurrentLimit Error: Failed to read REG02");
    }
    return -1;
}

String SY6974::getChargeStatus()
{
    uint8_t reg_data;
    if (readRegister(REG08_STATUS, reg_data))
    {
        uint8_t chrg_stat = (reg_data & 0x18) >> 3;
        switch (chrg_stat)
        {
        case 0x00:
            return "Not Charging";
        case 0x01:
            return "Pre-charge";
        case 0x02:
            return "Fast Charging";
        case 0x03:
            return "Charge Termination Done / Charging";
        }
    }
    return "Read Error";
}

String SY6974::getBusStatus()
{
    uint8_t reg_data;
    if (readRegister(REG08_STATUS, reg_data))
    {
        uint8_t bus_stat = (reg_data >> 5) & 0x07;

        switch (bus_stat)
        {
        case 0x00:
            return "No input";
        case 0x01:
            return "USB host SDP";
        case 0x02:
            return "USB CDP";
        case 0x03:
            return "USB DCP";
        case 0x04:
            return "Adjustable high voltage DCP (MaxCharge)";
        case 0x05:
            return "Unknown adapter";
        case 0x06:
            return "Non-standard adapter";
        case 0x07:
            return "OTG Mode";
        default:
            return "Unknown/Reserved";
        }
    }
    return "Read Error";
}

bool SY6974::isUsbAttached()
{
    uint8_t reg_data;
    if (readRegister(REG0A_BUS_GD, reg_data))
    {
        if ((reg_data & 0x80) != 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        if (log_)
        {
            log_->println("IsUsbAttached Error: Failed to read REG0A");
        }
        return false;
    }
}
