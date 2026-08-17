#pragma once

#include <stdint.h>

#include "TFT_eSPI.h"
#include "boards/board_registry.h"

class EpaperDisplay {
public:
    explicit EpaperDisplay(const board_registry::BoardScreenEntry& screen);

    void begin();
    EPaper& native();
    const EPaper& native() const;

    uint16_t width() const;
    uint16_t height() const;
    uint16_t comboId() const;
    bool isColor() const;
    const char* screenType() const;
    const char* resolution() const;
    const char* boardInfoType() const;
    uint8_t rotation(size_t orientation_index = 0) const;
    uint8_t orientationIndex() const;

private:
    EPaper display_;
    const board_registry::BoardScreenEntry& screen_;
};
