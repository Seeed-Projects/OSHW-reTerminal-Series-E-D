#pragma once

#include <stdint.h>
#include <stddef.h>
#include "driver.h"

#include "resources/fonts/Roboto-VariableFont.h"
#include "resources/fonts/SourceHanSansCN-Bold-2.h"

enum class ActivationCodeFontEngine
{
    kU8G2,
    kOpenFontRender
};

struct ActivationCodeTextStyle
{
    const uint8_t *u8g2Font;
    ActivationCodeFontEngine engine;
    const uint8_t *renderFontData;
    size_t renderFontDataSize;
    uint16_t renderFontSize;
};

struct ActivationCodeTextPosition
{
    int16_t x;
    int16_t baseline;
    bool centered;
};

struct ActivationCodeDigitsLayout
{
    ActivationCodeTextStyle textStyle;
    int16_t rowTop;
    int16_t startX;
    bool centerRow;
    int16_t boxWidth;
    int16_t boxHeight;
    int16_t boxCornerRadius;
    int16_t spacing;
    int16_t digitBaselineAdjust;
    int16_t connectorGap;
    int16_t connectorLength;
    int16_t connectorThickness;
};

struct ActivationCodePageLayout
{
    const char *titleText;
    ActivationCodeTextStyle titleStyle;
    ActivationCodeTextPosition titlePosition;

    ActivationCodeDigitsLayout digits;

    ActivationCodeTextStyle infoStyle;
    ActivationCodeTextPosition infoPosition;

    int16_t underlineOffset;
};

constexpr ActivationCodePageLayout MakeFallbackActivationCodePageLayout()
{
    return ActivationCodePageLayout{
        /*titleText*/ "Pair Code",
        /*titleStyle*/ ActivationCodeTextStyle{u8g2_font_60, ActivationCodeFontEngine::kU8G2, nullptr, 0, 0},
        /*titlePosition*/ ActivationCodeTextPosition{0, 140, true},
        /*digits*/ ActivationCodeDigitsLayout{
            ActivationCodeTextStyle{u8g2_font_42, ActivationCodeFontEngine::kU8G2, nullptr, 0, 0},
            /*rowTop*/ 200,
            /*startX*/ 80,
            /*centerRow*/ true,
            /*boxWidth*/ 90,
            /*boxHeight*/ 90,
            /*boxCornerRadius*/ 10,
            /*spacing*/ 30,
            /*digitBaselineAdjust*/ 4,
            /*connectorGap*/ 10,
            /*connectorLength*/ 0,
            /*connectorThickness*/ 4},
        /*infoStyle*/ ActivationCodeTextStyle{u8g2_font_helvR18_tf, ActivationCodeFontEngine::kU8G2, nullptr, 0, 0},
        /*infoPosition*/ ActivationCodeTextPosition{0, 350, true},
        /*underlineOffset*/ 4};
}

constexpr ActivationCodePageLayout MakeFallbackActivationCodePageLayout_800_480()
{
    return MakeFallbackActivationCodePageLayout();
}

constexpr ActivationCodePageLayout MakeFallbackActivationCodePageLayout_10_3_mono()
{
    return ActivationCodePageLayout{
        /*titleText*/ "Pair Code",
        /*titleStyle*/ ActivationCodeTextStyle{u8g2_font_60, ActivationCodeFontEngine::kOpenFontRender, sourcehansanscnbold2_otf_data, sizeof(sourcehansanscnbold2_otf_data), 150},
        /*titlePosition*/ ActivationCodeTextPosition{0, 460, true},
        /*digits*/ ActivationCodeDigitsLayout{
            ActivationCodeTextStyle{u8g2_font_logisoso92_tn, ActivationCodeFontEngine::kU8G2, nullptr, 0, 0},
            /*rowTop*/ 600,
            /*startX*/ 180,
            /*centerRow*/ true,
            /*boxWidth*/ 200,
            /*boxHeight*/ 230,
            /*boxCornerRadius*/ 16,
            /*spacing*/ 80,
            /*digitBaselineAdjust*/ 10,
            /*connectorGap*/ 20,
            /*connectorLength*/ 40,
            /*connectorThickness*/ 6},
        /*infoStyle*/ ActivationCodeTextStyle{u8g2_font_helvR24_tf, ActivationCodeFontEngine::kOpenFontRender, robotovariablefont_wdthwght_ttf_data, sizeof(robotovariablefont_wdthwght_ttf_data), 50},
        /*infoPosition*/ ActivationCodeTextPosition{0, 950, true},
        /*underlineOffset*/ 20};
}


constexpr ActivationCodePageLayout MakeFallbackActivationCodePageLayout_13_3_color()
{
    return ActivationCodePageLayout{
        /*titleText*/ "Pair Code",
        /*titleStyle*/ ActivationCodeTextStyle{u8g2_font_60, ActivationCodeFontEngine::kOpenFontRender, sourcehansanscnbold2_otf_data, sizeof(sourcehansanscnbold2_otf_data), 120},
        /*titlePosition*/ ActivationCodeTextPosition{0, 600, true},
        /*digits*/ ActivationCodeDigitsLayout{
            ActivationCodeTextStyle{u8g2_font_60, ActivationCodeFontEngine::kOpenFontRender, sourcehansanscnbold2_otf_data, sizeof(sourcehansanscnbold2_otf_data), 80},
            /*rowTop*/ 700,
            /*startX*/ 150,
            /*centerRow*/ true,
            /*boxWidth*/ 150,
            /*boxHeight*/ 150,
            /*boxCornerRadius*/ 20,
            /*spacing*/ 40,
            /*digitBaselineAdjust*/ -10,
            /*connectorGap*/ 5,
            /*connectorLength*/ 25,
            /*connectorThickness*/ 6},
        /*infoStyle*/ ActivationCodeTextStyle{u8g2_font_fur25_tf, ActivationCodeFontEngine::kU8G2, robotovariablefont_wdthwght_ttf_data, sizeof(robotovariablefont_wdthwght_ttf_data), 38},
        /*infoPosition*/ ActivationCodeTextPosition{0, 950, true},
        /*underlineOffset*/ 12};
}

#if (BOARD_SCREEN_COMBO == 520) || (BOARD_SCREEN_COMBO == 521) || (BOARD_SCREEN_COMBO == 515) || (BOARD_SCREEN_COMBO == 516)
constexpr ActivationCodePageLayout kActivationCodePageLayout = MakeFallbackActivationCodePageLayout_800_480();
#elif (BOARD_SCREEN_COMBO == 522 || BOARD_SCREEN_COMBO == 511)
constexpr ActivationCodePageLayout kActivationCodePageLayout = MakeFallbackActivationCodePageLayout_10_3_mono();
#elif (BOARD_SCREEN_COMBO == 523 || BOARD_SCREEN_COMBO == 510)
constexpr ActivationCodePageLayout kActivationCodePageLayout = MakeFallbackActivationCodePageLayout_13_3_color();
#else
constexpr ActivationCodePageLayout kActivationCodePageLayout = MakeFallbackActivationCodePageLayout();
#endif
