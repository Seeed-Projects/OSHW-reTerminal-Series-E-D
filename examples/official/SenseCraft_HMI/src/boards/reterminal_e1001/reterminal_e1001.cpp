#include "boards/common/board.h"

#include "ArduinoLog.h"
#include "config.h"
#include "TFT_eSPI.h"

namespace {

#if defined(BOARD_SEEED_RETERMINAL_E1002)
constexpr const char* BOARD_TYPE = "reterminal_e1002";
#else
constexpr const char* BOARD_TYPE = "reterminal_e1001";
#endif

class ReTerminalE1001Board : public Board {
public:
    ReTerminalE1001Board()
        : led_(GREENLED_PIN, LOW),
          button0_(KEY0_PIN, true),
          button1_(KEY1_PIN, true),
          button2_(KEY2_PIN, true),
          buzzer_(BUZZER_PIN),
          display_(board_registry::current_board_screen()),
          sd_({SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN, SD_EN_PIN, SD_DET_PIN}),
          pmic_(Wire1, Serial1, PMIC_I2C_SDA, PMIC_I2C_SCL),
          rtc_(Wire),
          env_(Wire),
          touch_(Wire, TOUCH_INT, TOUCH_RST),
          battery_({BAT_ADC_PIN, -1, ADC_EN, BATTERY_CURVE, BATTERY_CURVE_SIZE, BatteryGauge::PresenceMode::Pmic})
    {
    }

    const char* GetBoardType() const override { return BOARD_TYPE; }

    void InitSerial() override
    {
        Serial1.begin(115200, SERIAL_8N1, UART_RX0, UART_TX0);
    }

    Print& GetSerial() override { return Serial1; }
    void SetDebugOutput(bool enabled) override { Serial1.setDebugOutput(enabled); }

    void InitEarlyHardware() override
    {
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
        pmic_.initBus();

        pmic_.init();
        env_.init();
        rtc_.init();

        display_.begin();
        Log.infoln("[Board] ePaper display initialization succeed");

        sd_.useSpi(display_.native().getSPIinstance());
        sd_.init();

        battery_.init();
        touch_.init(display_.width(), display_.height());
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
    RtcPcf8563* GetRtc() override { return rtc_.isAvailable() ? &rtc_ : nullptr; }
    Sht40Sensor* GetEnv() override { return env_.isAvailable() ? &env_ : nullptr; }
    Gt911Touch* GetTouch() override { return touch_.isAvailable() ? &touch_ : nullptr; }
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
    RtcPcf8563 rtc_;
    Sht40Sensor env_;
    Gt911Touch touch_;
    BatteryGauge battery_;
};

}  // namespace

DECLARE_BOARD(ReTerminalE1001Board)
