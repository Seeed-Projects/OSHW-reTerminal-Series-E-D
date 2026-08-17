#pragma once

#include <stdint.h>

class EPaper;

namespace screen_assets {

void drawWaitingScreen(EPaper& display, uint16_t fg_color, uint16_t bg_color);
bool drawInitialCover(EPaper& display, bool device_bound);

}  // namespace screen_assets
