#include "boards/common/button.h"

Button::Button(int pin, bool active_low)
    : pin_(pin), active_low_(active_low), button_(pin, active_low)
{
}

bool Button::isPressed() const
{
    return digitalRead(pin_) == (active_low_ ? LOW : HIGH);
}

void Button::init()
{
    pinMode(pin_, active_low_ ? INPUT_PULLUP : INPUT);
}

void Button::tick()
{
    button_.tick();
}

void Button::reset()
{
    button_.reset();
}

void Button::setPressMs(uint32_t ms)
{
    button_.setPressMs(ms);
}

void Button::onClick(callbackFunction callback)
{
    button_.attachClick(callback);
}

void Button::onLongPressStart(callbackFunction callback)
{
    button_.attachLongPressStart(callback);
}

void Button::onLongPressStop(callbackFunction callback)
{
    button_.attachLongPressStop(callback);
}
