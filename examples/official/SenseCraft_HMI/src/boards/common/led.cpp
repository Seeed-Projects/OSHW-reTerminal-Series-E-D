#include "boards/common/led.h"

Led::Led(int pin, uint8_t on_level)
    : pin_(pin), on_level_(on_level)
{
}

void Led::init()
{
    pinMode(pin_, OUTPUT);
}

void Led::set(bool on)
{
    digitalWrite(pin_, on ? on_level_ : !on_level_);
}
