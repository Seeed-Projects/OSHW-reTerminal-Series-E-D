#pragma once

#include <Arduino.h>
#include <FS.h>
#include "GT911.h"

#include "boards/common/button.h"
#include "boards/common/epaper_display.h"
#include "boards/common/sht40_sensor.h"
#include "app_config.h"

class RtcDateTime;
class EPaper;

namespace HAL {

class SharedSpiLock {
public:
    explicit SharedSpiLock(bool enabled = true);
    ~SharedSpiLock();

    SharedSpiLock(const SharedSpiLock&) = delete;
    SharedSpiLock& operator=(const SharedSpiLock&) = delete;

private:
    bool locked_;
};

class Hal {
public:
    void init();

    const board_registry::BoardProfile& boardProfile() const;
    const board_registry::BoardScreenEntry& screen() const;
    const char* boardType() const;
    const char* bleShortName() const;
    const char* apPrefix() const;
    const char* screenResolution() const;
    const char* boardInfoType() const;
    const char* boardInfoScreenType() const;
    uint16_t screenWidth() const;
    uint16_t screenHeight() const;
    bool screenIsColor() const;
    uint8_t screenRotation(size_t orientation_index = 0) const;
    uint8_t screenOrientationIndex() const;

    void ledSet(bool on);
    Button* button(size_t index);
    bool buttonIsPressed(size_t index);
    void enableButtonWakeup();
    bool buzzerBeep(uint32_t frequency_hz, uint32_t duration_ms);
    void buzzerStop();

    bool sdInit();
    void sdDeinit();
    bool sdIsInserted();
    bool sdSupportsHotplugDetection();
    bool sdIsMounted();
    bool sdIsReady();
    bool sdEnsureReady();
    fs::File sdOpen(const char* path, const char* mode = FILE_READ);
    bool sdExists(const char* path);
    bool sdRemove(const char* path);
    uint64_t sdTotalBytes();
    uint64_t sdUsedBytes();

    bool batteryIsInserted();
    bool batteryIsConnected();
    float batteryReadVoltage();
    float batteryReadPercent();

    bool pmicIsCharging();
    bool pmicIsBatteryConnected();

    bool rtcReadTime(RtcDateTime& dt);
    bool rtcSetTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);

    SHT40_DATA envRead();

    void touchLoop();
    void touchOnGesture(GestureCallback callback);
    void touchEnableGestureWakeup(int active_level, uint32_t idle_timeout_ms);
    void touchEnableWakeup();

    EPaper& display();
    EpaperDisplay* epaperDisplay();
    WakeupClick wakeupReason() const;

private:
    WakeupClick wakeup_reason_ = WakeupClick::None;
};

Hal& GetHAL();

}  // namespace HAL
