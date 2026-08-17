#include "boards/common/board.h"

#include "ArduinoLog.h"
#include "config.h"
#include "TFT_eSPI.h"

namespace {

class ReTerminalE1004Board : public Board {
public:
    ReTerminalE1004Board()
        : led_(GREENLED_PIN, HIGH),
          button0_(KEY0_PIN, true),
          button1_(KEY1_PIN, true),
          button2_(KEY2_PIN, true),
          buzzer_(BUZZER_PIN),
          display_(board_registry::current_board_screen()),
          sd_({SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN, SD_EN_PIN, SD_DET_PIN}),
          pmic_(Wire, Serial1, PMIC_I2C_SDA, PMIC_I2C_SCL),
          env_(Wire),
          battery_({BAT_ADC_PIN, POWER_EN, ADC_EN, BATTERY_CURVE, BATTERY_CURVE_SIZE, BatteryGauge::PresenceMode::Pmic})
    {
    }

    const char* GetBoardType() const override { return "reterminal_e1004"; }

    void InitSerial() override
    {
        Serial1.begin(115200, SERIAL_8N1, UART_RX0, UART_TX0);
    }

    Print& GetSerial() override { return Serial1; }
    void SetDebugOutput(bool enabled) override { Serial1.setDebugOutput(enabled); }

    void InitEarlyHardware() override
    {
        pinMode(POWER_EN, OUTPUT);
        digitalWrite(POWER_EN, HIGH);
        led_.init();
        button0_.init();
        button1_.init();
        button2_.init();
        buzzer_.init();
        sd_.initPins();
    }

    void InitHardware() override
    {
        Wire.setPins(ESP32_SDA, ESP32_SCL);
        Wire.begin(ESP32_SDA, ESP32_SCL);
        Wire.setClock(400000);

        Wire1.setPins(ESP32_SDA1, ESP32_SCL1);
        Wire1.begin(ESP32_SDA1, ESP32_SCL1);
        Wire1.setClock(400000);

        pmic_.init();
        env_.init();

        display_.begin();
        Log.infoln("[Board] ePaper display initialization succeed");

        sd_.useSpi(display_.native().getSPIinstance());
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
    Buzzer* GetBuzzer() override { return buzzer_.isAvailable() ? &buzzer_ : nullptr; }
    EpaperDisplay* GetEpaperDisplay() override { return &display_; }
    SdCard* GetSdCard() override { return &sd_; }
    BatteryGauge* GetBattery() override { return &battery_; }
    PmicSy6974* GetPmic() override { return pmic_.isDetected() ? &pmic_ : nullptr; }
    Sht40Sensor* GetEnv() override { return env_.isAvailable() ? &env_ : nullptr; }
    EPaper& GetDisplay() override { return display_.native(); }

private:
    Led led_;
    Button button0_;
    Button button1_;
    Button button2_;
    Buzzer buzzer_;
    EpaperDisplay display_;
    SdCard sd_;
    PmicSy6974 pmic_;
    Sht40Sensor env_;
    BatteryGauge battery_;
};

}  // namespace

DECLARE_BOARD(ReTerminalE1004Board)
