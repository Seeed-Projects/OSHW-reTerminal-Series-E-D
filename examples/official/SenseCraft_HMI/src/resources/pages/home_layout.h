#pragma once

#include <stdint.h>
#include <stddef.h>
#include <array>
#include "driver.h"

#include "resources/fonts/u8g2_font_custom.h"
#include "resources/fonts/Roboto-VariableFont.h"
#include "resources/fonts/SourceHanSansCN-Bold-2.h"

enum class HomePageFontEngine
{
    kU8G2,
    kOpenFontRender
};

struct HomePageTextStyle
{
    const uint8_t *u8g2Font;
    HomePageFontEngine engine;
    const uint8_t *renderFontData;
    size_t renderFontDataSize;
    uint16_t renderFontSize;
};

struct HomePageInstructionStepPosition
{
    int16_t boxX;
    int16_t boxY;
    int16_t textX;
    int16_t textBaseline;
};

struct HomePageQrBlockPosition
{
    int16_t x;
    int16_t y;
    int16_t labelX;
    int16_t labelBaseline;
};

struct HomePageTextPosition
{
    int16_t x;
    int16_t baseline;
};

struct HomePageLayout
{
    // Header/logo region
    HomePageTextStyle headerStyle;
    HomePageTextPosition headerText;
    int16_t logoX;
    int16_t logoY;
    int16_t logoWidth;
    int16_t logoHeight;
    int16_t headerLineY;
    int16_t headerLineStartX;
    int16_t headerLineWidth;
    bool headerLineUseDisplayWidth;

    // QR block captions and spacing
    HomePageTextStyle qrLabelStyle;

    // Main hero text
    HomePageTextStyle mainTextStyle;
    HomePageTextPosition mainLine1;
    HomePageTextPosition mainLine2;

    // Instruction steps (numbered boxes + descriptions)
    HomePageTextStyle instructionNumberStyle;
    HomePageTextStyle instructionTextStyle;
    int16_t instructionBoxSize;
    int16_t instructionCornerRadius;
    std::array<HomePageInstructionStepPosition, 3> instructionSteps;

    // QR block explicit coordinates
    std::array<HomePageQrBlockPosition, 2> qrBlocks;
};

constexpr HomePageLayout MakeFallbackHomePageLayout()
{
    return HomePageLayout{
        /*Header*/ HomePageTextStyle{u8g2_font_36,
                                     HomePageFontEngine::kOpenFontRender,
                                     sourcehansanscnbold2_otf_data,
                                     sizeof(sourcehansanscnbold2_otf_data),
                                     42},
        /*headerText*/ HomePageTextPosition{20, 50},
        /*logoX*/ 560,
        /*logoY*/ 16,
        /*logoWidth*/ 231,
        /*logoHeight*/ 50,
        /*headerLineY*/ 80,
        /*headerLineStartX*/ 0,
        /*headerLineWidth*/ 0,
        /*headerLineUseDisplayWidth*/ true,

        /*qrLabelStyle*/ HomePageTextStyle{u8g2_font_t0_14_tf, HomePageFontEngine::kU8G2, nullptr, 0, 0},

        /*mainTextStyle*/ HomePageTextStyle{u8g2_font_42,HomePageFontEngine::kOpenFontRender, sourcehansanscnbold2_otf_data, sizeof(robotovariablefont_wdthwght_ttf_data), 38},
        /*mainLine1*/ HomePageTextPosition{20, 160},
        /*mainLine2*/ HomePageTextPosition{20, 240},

        /*instructionNumberStyle*/ HomePageTextStyle{u8g2_font_helvB14_tf, HomePageFontEngine::kU8G2, nullptr, 0, 0},
        /*instructionTextStyle*/ HomePageTextStyle{u8g2_font_helvR14_tf, HomePageFontEngine::kU8G2, sourcehansanscnbold2_otf_data, sizeof(robotovariablefont_wdthwght_ttf_data), 16},
        /*instructionBoxSize*/ 30,
        /*instructionCornerRadius*/ 6,
        /*instruction steps*/ std::array<HomePageInstructionStepPosition, 3>{
            HomePageInstructionStepPosition{30, 310, 75, 330},
            HomePageInstructionStepPosition{30, 365, 75, 385},
            HomePageInstructionStepPosition{30, 420, 75, 440}},

        /*QR blocks*/ std::array<HomePageQrBlockPosition, 2>{
            HomePageQrBlockPosition{610, 95, -1, 265},
            HomePageQrBlockPosition{610, 285, -1, 455}}
    };
}

constexpr HomePageLayout MakeFallbackHomePageLayout_800_480()
{
    return MakeFallbackHomePageLayout();
}

constexpr HomePageLayout MakeFallbackHomePageLayout_10_3_mono()
{
    return HomePageLayout{
        /*Header*/ HomePageTextStyle{u8g2_font_60,
                                     HomePageFontEngine::kOpenFontRender,
                                     sourcehansanscnbold2_otf_data,
                                     sizeof(sourcehansanscnbold2_otf_data),
                                     130},
        /*headerText*/ HomePageTextPosition{40, 160},
        /*logoX*/ 1270,
        /*logoY*/ 60,
        /*logoWidth*/ 600,
        /*logoHeight*/ 130,
        /*headerLineY*/ 250,
        /*headerLineStartX*/ 0,
        /*headerLineWidth*/ 0,
        /*headerLineUseDisplayWidth*/ true,

        /*qrLabelStyle*/ HomePageTextStyle{u8g2_font_24, HomePageFontEngine::kU8G2, nullptr, 0, 0},

        /*Main text style*/ HomePageTextStyle{u8g2_font_wqy16_t_gb2312, HomePageFontEngine::kOpenFontRender, sourcehansanscnbold2_otf_data, sizeof(robotovariablefont_wdthwght_ttf_data), 90},
        /*mainLine1*/ HomePageTextPosition{50, 460},
        /*mainLine2*/ HomePageTextPosition{50, 660},

        /*instructionNumberStyle*/ HomePageTextStyle{u8g2_font_helvB24_tf, HomePageFontEngine::kU8G2, sourcehansanscnbold2_otf_data, sizeof(robotovariablefont_wdthwght_ttf_data), 24},
        /*instructionTextStyle*/ HomePageTextStyle{u8g2_font_helvB24_tf, HomePageFontEngine::kOpenFontRender, sourcehansanscnbold2_otf_data, sizeof(robotovariablefont_wdthwght_ttf_data), 46},
        /*instructionBoxSize*/ 50,
        /*instructionCornerRadius*/ 8,
        std::array<HomePageInstructionStepPosition, 3>{
            HomePageInstructionStepPosition{50, 900, 140, 940},
            HomePageInstructionStepPosition{50, 1050, 140, 1090},
            HomePageInstructionStepPosition{50, 1200, 140, 1240}},

        std::array<HomePageQrBlockPosition, 2>{
            HomePageQrBlockPosition{1500, 420, -1, 710},
            HomePageQrBlockPosition{1500, 920, -1, 1210}}
    };
}

constexpr HomePageLayout MakeFallbackHomePageLayout_13_3_color()
{
    return HomePageLayout{
        /*Header*/ HomePageTextStyle{NULL,
                                     HomePageFontEngine::kOpenFontRender,
                                     sourcehansanscnbold2_otf_data,
                                     sizeof(sourcehansanscnbold2_otf_data),
                                     130},
        /*header text*/ HomePageTextPosition{50, 260},
        /*logoX*/ 30,
        /*logoY*/ 10,
        /*logoWidth*/ 600,
        /*logoHeight*/ 130,
        /*headerLineY*/ 380,
        /*headerLineStartX*/ 0,
        /*headerLineWidth*/ 0,
        /*headerLineUseDisplayWidth*/ true,

        /*QR label style*/ HomePageTextStyle{u8g2_font_fub25_tr, HomePageFontEngine::kU8G2, nullptr, 0, 0},

        /*Main text style*/ HomePageTextStyle{NULL, HomePageFontEngine::kOpenFontRender, sourcehansanscnbold2_otf_data, sizeof(robotovariablefont_wdthwght_ttf_data), 72},
        /*line1*/ HomePageTextPosition{60, 510},
        /*line2*/ HomePageTextPosition{60, 670},

        /*Instruction number style*/ HomePageTextStyle{u8g2_font_helvB24_tf, HomePageFontEngine::kU8G2, nullptr, 0, 0},
        /*Instruction text style*/ HomePageTextStyle{NULL, HomePageFontEngine::kOpenFontRender, sourcehansanscnbold2_otf_data, sizeof(sourcehansanscnbold2_otf_data), 36},
        /*instructionBoxSize*/ 40,
        /*instructionCornerRadius*/ 5,
        std::array<HomePageInstructionStepPosition, 3>{
            HomePageInstructionStepPosition{50, 900+60, 50+60, 930+60},
            HomePageInstructionStepPosition{50, 980+60, 50+60, 1010+60},
            HomePageInstructionStepPosition{50, 1060+60, 50+60, 1090+60}},

        std::array<HomePageQrBlockPosition, 2>{
            HomePageQrBlockPosition{200, 1250, -1, -1},  // Left QR block (x, y, optional label X/baseline)
            HomePageQrBlockPosition{740, 1250, -1, -1}}
    };
}

#if (BOARD_SCREEN_COMBO == 520) || (BOARD_SCREEN_COMBO == 521) || (BOARD_SCREEN_COMBO == 515) || (BOARD_SCREEN_COMBO == 516)
constexpr HomePageLayout kHomePageLayout = MakeFallbackHomePageLayout_800_480();
#elif (BOARD_SCREEN_COMBO == 522) || (BOARD_SCREEN_COMBO == 511)
constexpr HomePageLayout kHomePageLayout = MakeFallbackHomePageLayout_10_3_mono();
#elif (BOARD_SCREEN_COMBO == 523) || (BOARD_SCREEN_COMBO == 510)
constexpr HomePageLayout kHomePageLayout = MakeFallbackHomePageLayout_13_3_color();
#else
constexpr HomePageLayout kHomePageLayout = MakeFallbackHomePageLayout();
#endif
