#include "boards/common/board.h"

#include "ArduinoLog.h"
#include "config.h"
#include "TFT_eSPI.h"

namespace {

BatteryGauge::PresenceMode battery_presence_mode()
{
#if defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE02) || defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE03)
    return BatteryGauge::PresenceMode::Ripple50;
#else
    return BatteryGauge::PresenceMode::Ripple10;
#endif
}

class XiaoDiyBoard : public Board {
public:
    XiaoDiyBoard()
        : led_(GREENLED_PIN, LOW),
          button0_(KEY0_PIN, true),
          button1_(KEY1_PIN, true),
          button2_(KEY2_PIN, true),
          display_(board_registry::current_board_screen()),
          sd_({SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN, SD_EN_PIN, SD_DET_PIN}),
          pmic_(Wire, Serial, PMIC_I2C_SDA, PMIC_I2C_SCL),
          battery_({BAT_ADC_PIN, POWER_EN, ADC_EN, BATTERY_CURVE, BATTERY_CURVE_SIZE, battery_presence_mode()})
    {
    }

    const char* GetBoardType() const override { return "xiao_diy"; }

    void InitSerial() override
    {
        Serial.begin(115200);
    }

    Print& GetSerial() override { return Serial; }
    void SetDebugOutput(bool enabled) override { Serial.setDebugOutput(enabled); }

    void InitEarlyHardware() override
    {
        pinMode(POWER_EN, OUTPUT);
        digitalWrite(POWER_EN, HIGH);
        led_.init();
        button0_.init();
        button1_.init();
        button2_.init();
        sd_.initPins();
    }

    void InitHardware() override
    {
        Wire.setPins(ESP32_SDA, ESP32_SCL);
        Wire.begin(ESP32_SDA, ESP32_SCL);
        Wire.setClock(400000);

        pmic_.init();

        display_.begin();
        Log.infoln("[Board] ePaper display initialization succeed");

        sd_.init();
        battery_.init();
    }

    Led* GetLed() override { return &led_; }
    Button* GetButton(size_t index) override
    {
        switch (index) {
        case 0:
            return &button0_;
        case 1:
            return &button1_;
        case 2:
            return &button2_;
        default:
            return nullptr;
        }
    }
    EpaperDisplay* GetEpaperDisplay() override { return &display_; }
    SdCard* GetSdCard() override { return &sd_; }
    BatteryGauge* GetBattery() override { return &battery_; }
    PmicSy6974* GetPmic() override { return pmic_.isDetected() ? &pmic_ : nullptr; }
    EPaper& GetDisplay() override { return display_.native(); }

private:
    Led led_;
    Button button0_;
    Button button1_;
    Button button2_;
    EpaperDisplay display_;
    SdCard sd_;
    PmicSy6974 pmic_;
    BatteryGauge battery_;
};

}  // namespace

DECLARE_BOARD(XiaoDiyBoard)
