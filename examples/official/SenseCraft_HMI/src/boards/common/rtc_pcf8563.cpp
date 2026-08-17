#include "boards/common/rtc_pcf8563.h"

#include "ArduinoLog.h"

RtcPcf8563::RtcPcf8563(TwoWire& wire)
    : rtc_(wire)
{
}

bool RtcPcf8563::init()
{
    initialized_ = false;

    rtc_.Begin();
    if (hasError("Rtc.Begin")) {
        return false;
    }

#if defined(WIRE_HAS_TIMEOUT)
    Wire.setWireTimeout(3000, true);
#endif

    if (!rtc_.GetIsRunning()) {
        if (hasError("GetIsRunning")) {
            return false;
        }

        Log.infoln("RTC is not running, starting now.");
        rtc_.SetIsRunning(true);
        if (hasError("SetIsRunning")) {
            return false;
        }
    }

    if (rtc_.LastError() != 0) {
        return false;
    }

    Log.infoln("[Board] RTC initialization successful");
    initialized_ = true;
    return true;
}

bool RtcPcf8563::readTime(RtcDateTime& dt)
{
    if (!initialized_) {
        return false;
    }

    if (!rtc_.IsDateTimeValid()) {
        Log.warningln("Warning: RTC clock data may be invalid.");
    }

    dt = rtc_.GetDateTime();
    if (hasError("GetDateTime")) {
        return false;
    }
    if (dt.Year() < 2025 && dt.Month() < 9) {
        Log.warningln("Warning: RTC clock data seems invalid, please set the time.");
        return false;
    }
    return true;
}

bool RtcPcf8563::setTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    if (!initialized_) {
        return false;
    }

    RtcDateTime rtc_data(year, month, day, hour, minute, second);
    rtc_.SetDateTime(rtc_data);
    return !hasError("SetDateTime");
}

bool RtcPcf8563::hasError(const char* topic)
{
    uint8_t error = rtc_.LastError();
    if (error == 0) {
        return false;
    }

    Log.warningln("[%s] I2C communication error (code: %u)", topic, error);
    return true;
}
