#include "hal/hal.h"

#include "ArduinoLog.h"
#include "boards/common/board.h"
#include "esp_log.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <RtcPCF8563.h>
#include "TFT_eSPI.h"

namespace HAL {

namespace {
constexpr float NO_BATTERY_DETECTED_VALUE = -1.0f;
constexpr int TOUCH_WAKEUP_ACTIVE_LEVEL = 1;
constexpr uint32_t TOUCH_WAKEUP_IDLE_TIMEOUT_MS = 300;

#define RETERMINAL \
"\n" \
"                  ________                                  __                      __\n" \
"                 |        \\                                |  \\                    |  \\\n" \
"  ______    ______\\$$$$$$$$______    ______   ______ ____   \\$$ _______    ______  | $$\n" \
" /      \\  /      \\ | $$  /      \\  /      \\ |      \\    \\ |  \\|       \\  |      \\ | $$\n" \
"|  $$$$$$\\|  $$$$$$\\| $$ |  $$$$$$\\|  $$$$$$\\| $$$$$$\\$$$$\\| $$| $$$$$$$\\  \\$$$$$$\\| $$\n" \
"| $$   \\$$| $$    $$| $$ | $$    $$| $$   \\$$| $$ | $$ | $$| $$| $$  | $$ /      $$| $$\n" \
"| $$      | $$$$$$$$| $$ | $$$$$$$$| $$      | $$ | $$ | $$| $$| $$  | $$|  $$$$$$$| $$\n" \
"| $$       \\$$     \\| $$  \\$$     \\| $$      | $$ | $$ | $$| $$| $$  | $$ \\$$    $$| $$\n" \
" \\$$        \\$$$$$$$ \\$$   \\$$$$$$$ \\$$       \\$$  \\$$  \\$$ \\$$ \\$$   \\$$  \\$$$$$$$ \\$$\n\n"

void printDateTime(const RtcDateTime& dt)
{
    char datestring[26];
    snprintf_P(datestring,
               sizeof(datestring),
               PSTR("[HAL] %04u-%02u-%02u %02u:%02u:%02u"),
               dt.Year(),
               dt.Month(),
               dt.Day(),
               dt.Hour(),
               dt.Minute(),
               dt.Second());
    Log.infoln(datestring);
}
}

Hal& GetHAL()
{
    static Hal hal;
    return hal;
}

SharedSpiLock::SharedSpiLock(bool enabled)
    : locked_(enabled)
{
    if (locked_) {
        SdCard::lockSharedBus();
        auto* sd = Board::GetInstance().GetSdCard();
        if (sd) {
            sd->releaseBus();
        }
    }
}

SharedSpiLock::~SharedSpiLock()
{
    if (locked_) {
        SdCard::unlockSharedBus();
    }
}

void Hal::init()
{
    auto& board = Board::GetInstance();
    board.InitSerial();
    auto& serial = board.GetSerial();

#if RETERMINAL_DEBUG
    Log.begin(LOG_LEVEL_VERBOSE, &serial);
    board.SetDebugOutput(true);
    esp_log_level_set("*", ESP_LOG_VERBOSE);
#else
    Log.begin(LOG_LEVEL_INFO, &serial);
#endif
    board.InitEarlyHardware();

    ledSet(true);
    Log.infoln("[Device info] turn on LED");

    serial.print(RETERMINAL);
    Log.infoln("[Device info] MAC: %s, App version: %s", WiFi.macAddress().c_str(), g_currentAppVersion);
    Log.infoln("[Device info] Firmware build: %s", FIRMWARE_BUILD_TIMESTAMP);
    Log.infoln("[Device info] Firmware environment: %s", HMI_FIRMWARE_ENV_MARKER);

    if (!LittleFS.begin(true)) {
        Log.errorln("[HAL] File system Mount Failed");
        return;
    }
    Log.infoln("[HAL] File system Mount Successful");

    board.InitHardware();

    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    if (reason == ESP_SLEEP_WAKEUP_EXT1 || reason == ESP_SLEEP_WAKEUP_EXT0) {
        delay(1000);
    }

    bool key0_pressed = buttonIsPressed(0);
    bool key1_pressed = buttonIsPressed(1);
    bool key2_pressed = buttonIsPressed(2);

    if (key0_pressed && key1_pressed && key2_pressed) {
        wakeup_reason_ = WakeupClick::TripleLong;
    } else if (key1_pressed && key2_pressed) {
        wakeup_reason_ = WakeupClick::DoubleLong;
    } else if (reason == ESP_SLEEP_WAKEUP_EXT0) {
        wakeup_reason_ = WakeupClick::Touch;
    } else if (reason == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t mask = esp_sleep_get_ext1_wakeup_status();

        auto* key2 = board.GetButton(2);
        auto* key1 = board.GetButton(1);
        auto* key0 = board.GetButton(0);

        if (key2 && (mask & (1ULL << key2->pin()))) {
            wakeup_reason_ = WakeupClick::Key2Short;
        }
        if (key1 && (mask & (1ULL << key1->pin()))) {
            wakeup_reason_ = WakeupClick::Key1Short;
        }
        if (key0 && (mask & (1ULL << key0->pin()))) {
            wakeup_reason_ = WakeupClick::Key0Short;
        }
    } else if (reason == ESP_SLEEP_WAKEUP_TIMER) {
        wakeup_reason_ = WakeupClick::Timer;
    } else {
        wakeup_reason_ = WakeupClick::None;
    }

    if (board.GetEnv()) {
        SHT40_DATA data = envRead();
        Log.infoln("[Device info] Sensor temperature: %F °C, humidity: %F %%", data.temperature, data.humidity);
    }

    if (board.GetRtc()) {
        RtcDateTime dt;
        if (rtcReadTime(dt)) {
            printDateTime(dt);
        }
    }

    float pct = batteryReadPercent();
    int pctInt = static_cast<int>(pct + 0.5f);
    Log.infoln("[Device info] Device battery level: %d", pctInt);

#if (LED_ALWAYS_ON)
#else
    ledSet(false);
#endif
}

const board_registry::BoardProfile& Hal::boardProfile() const
{
    return Board::GetInstance().GetProfile();
}

const board_registry::BoardScreenEntry& Hal::screen() const
{
    return Board::GetInstance().GetScreen();
}

const char* Hal::boardType() const
{
    return Board::GetInstance().GetBoardType();
}

const char* Hal::bleShortName() const
{
    return boardProfile().ble_short_name;
}

const char* Hal::apPrefix() const
{
    return boardProfile().ap_prefix;
}

const char* Hal::screenResolution() const
{
    return screen().resolution;
}

const char* Hal::boardInfoType() const
{
    return screen().board_info_type;
}

const char* Hal::boardInfoScreenType() const
{
    return screen().board_info_screen_type;
}

uint16_t Hal::screenWidth() const
{
    return screen().width;
}

uint16_t Hal::screenHeight() const
{
    return screen().height;
}

bool Hal::screenIsColor() const
{
    return screen().color == board_registry::ScreenColor::Chromatic;
}

uint8_t Hal::screenRotation(size_t orientation_index) const
{
    if (orientation_index >= 4) {
        orientation_index = 0;
    }
    return screen().rotation_map[orientation_index];
}

uint8_t Hal::screenOrientationIndex() const
{
    return board_registry::combo_orientation_index(screen().combo_id);
}

void Hal::ledSet(bool on)
{
    auto* led = Board::GetInstance().GetLed();
    if (led) {
        led->set(on);
    }
}

Button* Hal::button(size_t index)
{
    return Board::GetInstance().GetButton(index);
}

bool Hal::buttonIsPressed(size_t index)
{
    auto* btn = button(index);
    return btn && btn->isPressed();
}

void Hal::enableButtonWakeup()
{
    uint64_t mask = 0;
    for (size_t i = 0; i < 3; ++i) {
        auto* btn = button(i);
        if (btn) {
            mask |= (1ULL << btn->pin());
        }
    }
    if (mask) {
        esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    }
}

bool Hal::buzzerBeep(uint32_t frequency_hz, uint32_t duration_ms)
{
    auto* buzzer = Board::GetInstance().GetBuzzer();
    return buzzer && buzzer->beep(frequency_hz, duration_ms);
}

void Hal::buzzerStop()
{
    auto* buzzer = Board::GetInstance().GetBuzzer();
    if (buzzer) {
        buzzer->stop();
    }
}

bool Hal::sdInit()
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd && sd->init();
}

void Hal::sdDeinit()
{
    auto* sd = Board::GetInstance().GetSdCard();
    if (sd) {
        sd->deinit();
    }
}

bool Hal::sdIsInserted()
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd && sd->isInserted();
}

bool Hal::sdSupportsHotplugDetection()
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd && sd->supportsHotplugDetection();
}

bool Hal::sdIsMounted()
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd && sd->isMounted();
}

bool Hal::sdIsReady()
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd && sd->isReady();
}

bool Hal::sdEnsureReady()
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd && sd->ensureReady();
}

fs::File Hal::sdOpen(const char* path, const char* mode)
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd ? sd->open(path, mode) : fs::File();
}

bool Hal::sdExists(const char* path)
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd && sd->exists(path);
}

bool Hal::sdRemove(const char* path)
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd && sd->remove(path);
}

uint64_t Hal::sdTotalBytes()
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd ? sd->totalBytes() : 0;
}

uint64_t Hal::sdUsedBytes()
{
    auto* sd = Board::GetInstance().GetSdCard();
    return sd ? sd->usedBytes() : 0;
}

bool Hal::batteryIsInserted()
{
    auto* battery = Board::GetInstance().GetBattery();
    return battery && battery->isInserted(pmicIsBatteryConnected());
}

bool Hal::batteryIsConnected()
{
    auto* battery = Board::GetInstance().GetBattery();
    return battery && battery->isConnected(pmicIsBatteryConnected());
}

float Hal::batteryReadVoltage()
{
    auto* battery = Board::GetInstance().GetBattery();
    if (!battery) {
        return NO_BATTERY_DETECTED_VALUE;
    }
    return battery->readVoltage(pmicIsBatteryConnected());
}

float Hal::batteryReadPercent()
{
    auto* battery = Board::GetInstance().GetBattery();
    if (!battery) {
        return NO_BATTERY_DETECTED_VALUE;
    }
    return battery->readPercent(pmicIsBatteryConnected());
}

bool Hal::pmicIsCharging()
{
    auto* pmic = Board::GetInstance().GetPmic();
    return pmic && pmic->isCharging();
}

bool Hal::pmicIsBatteryConnected()
{
    auto* pmic = Board::GetInstance().GetPmic();
    return !pmic || pmic->isBatteryConnected();
}

bool Hal::rtcReadTime(RtcDateTime& dt)
{
    auto* rtc = Board::GetInstance().GetRtc();
    return rtc && rtc->readTime(dt);
}

bool Hal::rtcSetTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    auto* rtc = Board::GetInstance().GetRtc();
    return rtc && rtc->setTime(year, month, day, hour, minute, second);
}

SHT40_DATA Hal::envRead()
{
    auto* env = Board::GetInstance().GetEnv();
    if (!env) {
        return {0.0f, 0.0f};
    }
    return env->read();
}

void Hal::touchLoop()
{
    auto* touch = Board::GetInstance().GetTouch();
    if (touch) {
        touch->loop();
    }
}

void Hal::touchOnGesture(GestureCallback callback)
{
    auto* touch = Board::GetInstance().GetTouch();
    if (touch) {
        touch->onGesture(callback);
    }
}

void Hal::touchEnableGestureWakeup(int active_level, uint32_t idle_timeout_ms)
{
    auto* touch = Board::GetInstance().GetTouch();
    if (touch) {
        touch->enableGestureWakeup(active_level, idle_timeout_ms);
    }
}

void Hal::touchEnableWakeup()
{
    touchEnableGestureWakeup(TOUCH_WAKEUP_ACTIVE_LEVEL, TOUCH_WAKEUP_IDLE_TIMEOUT_MS);
}

EPaper& Hal::display()
{
    return Board::GetInstance().GetDisplay();
}

EpaperDisplay* Hal::epaperDisplay()
{
    return Board::GetInstance().GetEpaperDisplay();
}

WakeupClick Hal::wakeupReason() const
{
    return wakeup_reason_;
}

}  // namespace HAL
