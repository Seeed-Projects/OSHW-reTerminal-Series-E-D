#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "boards/common/battery_gauge.h"
#include "boards/common/button.h"
#include "boards/common/buzzer.h"
#include "boards/common/epaper_display.h"
#include "boards/common/gt911_touch.h"
#include "boards/common/led.h"
#include "boards/common/pmic_sy6974.h"
#include "boards/common/rtc_pcf8563.h"
#include "boards/common/sd_card.h"
#include "boards/common/sht40_sensor.h"
#include "boards/board_registry.h"

class EPaper;

void* create_board();

class Board {
public:
    static Board& GetInstance();
    virtual ~Board() = default;

    virtual const char* GetBoardType() const = 0;
    virtual void InitSerial() = 0;
    virtual Print& GetSerial() = 0;
    virtual void SetDebugOutput(bool) {}
    virtual void InitEarlyHardware() {}
    virtual void InitHardware() {}

    virtual const board_registry::BoardProfile& GetProfile() const { return board_registry::current_board(); }
    virtual const board_registry::BoardScreenEntry& GetScreen() const { return board_registry::current_board_screen(); }

    virtual Led* GetLed() { return nullptr; }
    virtual Button* GetButton(size_t) { return nullptr; }
    virtual Buzzer* GetBuzzer() { return nullptr; }
    virtual EpaperDisplay* GetEpaperDisplay() { return nullptr; }
    virtual SdCard* GetSdCard() { return nullptr; }
    virtual BatteryGauge* GetBattery() { return nullptr; }
    virtual PmicSy6974* GetPmic() { return nullptr; }
    virtual RtcPcf8563* GetRtc() { return nullptr; }
    virtual Sht40Sensor* GetEnv() { return nullptr; }
    virtual Gt911Touch* GetTouch() { return nullptr; }
    virtual EPaper& GetDisplay() = 0;
};

#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { return new BOARD_CLASS_NAME(); }
