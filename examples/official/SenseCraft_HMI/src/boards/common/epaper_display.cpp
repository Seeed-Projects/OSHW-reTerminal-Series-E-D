#include "boards/common/epaper_display.h"

EpaperDisplay::EpaperDisplay(const board_registry::BoardScreenEntry& screen)
    : screen_(screen)
{
}

void EpaperDisplay::begin()
{
    display_.begin();
}

EPaper& EpaperDisplay::native()
{
    return display_;
}

const EPaper& EpaperDisplay::native() const
{
    return display_;
}

uint16_t EpaperDisplay::width() const
{
    return screen_.width;
}

uint16_t EpaperDisplay::height() const
{
    return screen_.height;
}

uint16_t EpaperDisplay::comboId() const
{
    return screen_.combo_id;
}

bool EpaperDisplay::isColor() const
{
    return screen_.color == board_registry::ScreenColor::Chromatic;
}

const char* EpaperDisplay::screenType() const
{
    return screen_.board_info_screen_type;
}

const char* EpaperDisplay::resolution() const
{
    return screen_.resolution;
}

const char* EpaperDisplay::boardInfoType() const
{
    return screen_.board_info_type;
}

uint8_t EpaperDisplay::rotation(size_t orientation_index) const
{
    if (orientation_index >= 4) {
        orientation_index = 0;
    }
    return screen_.rotation_map[orientation_index];
}

uint8_t EpaperDisplay::orientationIndex() const
{
    return board_registry::combo_orientation_index(screen_.combo_id);
}
