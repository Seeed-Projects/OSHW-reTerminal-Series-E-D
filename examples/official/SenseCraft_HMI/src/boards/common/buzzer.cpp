#include "boards/common/buzzer.h"

namespace
{
constexpr uint8_t BUZZER_LEDC_CHANNEL = 7;
constexpr uint8_t BUZZER_LEDC_RESOLUTION_BITS = 10;
constexpr uint32_t BUZZER_IDLE_FREQ_HZ = 1000;
}

Buzzer::Buzzer(int pin)
    : pin_(pin)
{
}

bool Buzzer::init()
{
    if (pin_ < 0) {
        initialized_ = false;
        return false;
    }

    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);

    if (ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_IDLE_FREQ_HZ, BUZZER_LEDC_RESOLUTION_BITS) == 0) {
        initialized_ = false;
        return false;
    }
    ledcAttachPin(pin_, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);

    initialized_ = true;
    return true;
}

bool Buzzer::beep(uint32_t frequency_hz, uint32_t)
{
    if (!initialized_ || frequency_hz == 0) {
        return false;
    }

    return ledcWriteTone(BUZZER_LEDC_CHANNEL, frequency_hz) != 0;
}

void Buzzer::stop()
{
    if (initialized_) {
        ledcWriteTone(BUZZER_LEDC_CHANNEL, 0);
    }
}
