#pragma once

#include <Arduino.h>
#include <OneButton.h>

class Button {
public:
    Button(int pin, bool active_low = true);

    int pin() const { return pin_; }
    bool isPressed() const;

    void init();
    void tick();
    void reset();
    void setPressMs(uint32_t ms);

    void onClick(callbackFunction callback);
    void onLongPressStart(callbackFunction callback);
    void onLongPressStop(callbackFunction callback);

private:
    int pin_;
    bool active_low_;
    OneButton button_;
};
