#include "boards/common/screen_assets.h"

#include "TFT_eSPI.h"
#include "driver.h"
#include "resources/pages/e1001_epd.h"
#include "resources/pages/e1002_epd.h"
#include "resources/pages/e1003_epd.h"
#include "resources/pages/e1004_epd.h"
#include "resources/pages/ee02.h"
#include "resources/pages/xiao_diy_kit.h"

namespace screen_assets {

void drawWaitingScreen(EPaper& display, uint16_t fg_color, uint16_t bg_color)
{
#if (BOARD_SCREEN_COMBO == 520)
    display.drawBitmap(0, 0, e1001, display.width(), display.height(), TFT_WHITE, TFT_BLACK);
    display.update();
#elif (BOARD_SCREEN_COMBO == 521)
    display.pushImage(0, 0, display.width(), display.height(), (uint16_t*)e1002);
    display.update();
#elif (BOARD_SCREEN_COMBO == 522)
    display.drawBitmap(0, 0, e1003, display.width(), display.height(), TFT_WHITE, TFT_BLACK);
    display.update();
#elif (BOARD_SCREEN_COMBO == 523)
    display.pushImage(0, 0, display.width(), display.height(), (uint16_t*)e1004);
    display.update();
#elif (BOARD_SCREEN_COMBO == 506) || (BOARD_SCREEN_COMBO == 502) || (BOARD_SCREEN_COMBO == 515)
    display.update(0, 0, display.width(), display.height(), (uint16_t*)e1001);
#elif (BOARD_SCREEN_COMBO == 509) || (BOARD_SCREEN_COMBO == 516)
    display.update(0, 0, display.width(), display.height(), (uint16_t*)e1002);
#elif (BOARD_SCREEN_COMBO == 510)
    display.pushImage(0, 0, display.width(), display.height(), (const uint16_t*)xiao_diy_kit);
    display.update();
#else
    display.drawBitmap(0, 0, xiao_diy_kit, display.width(), display.height(), fg_color, bg_color);
    display.update();
#endif
}

bool drawInitialCover(EPaper& display, bool device_bound)
{
#if (BOARD_SCREEN_COMBO == 510) || (BOARD_SCREEN_COMBO == 511)
    if (device_bound) {
        return false;
    }

    display.fillScreen(TFT_WHITE);
#if (BOARD_SCREEN_COMBO == 510)
    display.pushImage(0, 0, display.width(), display.height(), (const uint16_t*)ee02cover);
#else
    display.pushImage(0, 0, display.width(), display.height(), (const uint16_t*)ee03cover);
#endif
    display.update();
    return true;
#else
    return false;
#endif
}

}  // namespace screen_assets
