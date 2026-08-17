#pragma once

#include <stdint.h>

#include "driver.h"
#include "resources/pages/diy_layout_common.h"

struct DIYActivationCodePageLayout
{
    uint16_t backgroundColor;
    uint16_t textColor;
    const uint8_t *titleFont;
    const char *titleText;
    DIYTextAlignment titleAlignment;
    int16_t titleTopPadding;
    int16_t titleLeftMargin;

    const uint8_t *codeFont;
    DIYTextAlignment codeAlignment;
    int16_t codeGapBelowTitle;
    int16_t codeLeftMargin;
    uint16_t codeTextColor;

    const uint8_t *infoFont;
    const char *infoText;
    DIYTextAlignment infoAlignment;
    int16_t infoGapBelowCode;
    int16_t infoLeftMargin;
    int16_t infoBottomOffset;

    const uint8_t *urlFont;
    DIYTextAlignment urlAlignment;
    int16_t urlGapBelowInfo;
    int16_t urlLeftMargin;
    int16_t urlBottomOffset;
    int16_t urlUnderlineOffset;
};

constexpr DIYActivationCodePageLayout MakeFallbackDIYActivationCodePageLayout()
{
    return DIYActivationCodePageLayout{
        /*backgroundColor=*/TFT_BLACK,
        /*textColor=*/TFT_WHITE,
        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"Pair Code",
        /*titleAlignment=*/DIYTextAlignment::kCenter,
        /*titleTopPadding=*/32,
        /*titleLeftMargin=*/0,

        /*codeFont=*/u8g2_font_36,
        /*codeAlignment=*/DIYTextAlignment::kCenter,
        /*codeGapBelowTitle=*/26,
        /*codeLeftMargin=*/0,
        /*codeTextColor=*/TFT_WHITE,

        /*infoFont=*/u8g2_font_helvR14_tf,
        /*infoText=*/"Add device on",
        /*infoAlignment=*/DIYTextAlignment::kCenter,
        /*infoGapBelowCode=*/24,
        /*infoLeftMargin=*/0,
        /*infoBottomOffset=*/-1,

        /*urlFont=*/u8g2_font_helvR14_tf,
        /*urlAlignment=*/DIYTextAlignment::kCenter,
        /*urlGapBelowInfo=*/12,
        /*urlLeftMargin=*/0,
        /*urlBottomOffset=*/-1,
        /*urlUnderlineOffset=*/2};
}

constexpr DIYActivationCodePageLayout MakeFallbackDIYActivationCodePageLayout_1_54()
{
    return DIYActivationCodePageLayout{
        /*backgroundColor=*/TFT_BLACK,
        /*textColor=*/TFT_WHITE,
        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"Pair Code",
        /*titleAlignment=*/DIYTextAlignment::kCenter,
        /*titleTopPadding=*/20,
        /*titleLeftMargin=*/0,

        /*codeFont=*/u8g2_font_36,
        /*codeAlignment=*/DIYTextAlignment::kCenter,
        /*codeGapBelowTitle=*/26 + 12,
        /*codeLeftMargin=*/0,
        /*codeTextColor=*/TFT_WHITE,

        /*infoFont=*/u8g2_font_helvR14_tf,
        /*infoText=*/"Add device on",
        /*infoAlignment=*/DIYTextAlignment::kLeft,
        /*infoGapBelowCode=*/45,
        /*infoLeftMargin=*/5,
        /*infoBottomOffset=*/-1,

        /*urlFont=*/u8g2_font_helvR08_tf,
        /*urlAlignment=*/DIYTextAlignment::kLeft,
        /*urlGapBelowInfo=*/8,
        /*urlLeftMargin=*/5,
        /*urlBottomOffset=*/-1,
        /*urlUnderlineOffset=*/2};
}

constexpr DIYActivationCodePageLayout MakeFallbackDIYActivationCodePageLayout_2_13()
{
    return DIYActivationCodePageLayout{
        /*backgroundColor=*/TFT_BLACK,
        /*textColor=*/TFT_WHITE,
        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"Pair Code",
        /*titleAlignment=*/DIYTextAlignment::kLeft,
        /*titleTopPadding=*/5,
        /*titleLeftMargin=*/5,

        /*codeFont=*/u8g2_font_36,
        /*codeAlignment=*/DIYTextAlignment::kLeft,
        /*codeGapBelowTitle=*/12,
        /*codeLeftMargin=*/5,
        /*codeTextColor=*/TFT_WHITE,

        /*infoFont=*/u8g2_font_helvR14_tf,
        /*infoText=*/"Add device on",
        /*infoAlignment=*/DIYTextAlignment::kLeft,
        /*infoGapBelowCode=*/15,
        /*infoLeftMargin=*/5,
        /*infoBottomOffset=*/-1,

        /*urlFont=*/u8g2_font_helvR10_tf,
        /*urlAlignment=*/DIYTextAlignment::kLeft,
        /*urlGapBelowInfo=*/8,
        /*urlLeftMargin=*/5,
        /*urlBottomOffset=*/-1,
        /*urlUnderlineOffset=*/2};
}

constexpr DIYActivationCodePageLayout MakeFallbackDIYActivationCodePageLayout_2_13_bwry()
{
    return DIYActivationCodePageLayout{
        /*backgroundColor=*/TFT_RED,
        /*textColor=*/TFT_WHITE,
        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"Pair Code",
        /*titleAlignment=*/DIYTextAlignment::kLeft,
        /*titleTopPadding=*/10,
        /*titleLeftMargin=*/5,

        /*codeFont=*/u8g2_font_36,
        /*codeAlignment=*/DIYTextAlignment::kLeft,
        /*codeGapBelowTitle=*/12,
        /*codeLeftMargin=*/5,
        /*codeTextColor=*/TFT_YELLOW,

        /*infoFont=*/u8g2_font_helvR14_tf,
        /*infoText=*/"Add device on",
        /*infoAlignment=*/DIYTextAlignment::kLeft,
        /*infoGapBelowCode=*/15,
        /*infoLeftMargin=*/5,
        /*infoBottomOffset=*/-1,

        /*urlFont=*/u8g2_font_helvR10_tf,
        /*urlAlignment=*/DIYTextAlignment::kLeft,
        /*urlGapBelowInfo=*/8,
        /*urlLeftMargin=*/5,
        /*urlBottomOffset=*/-1,
        /*urlUnderlineOffset=*/2};
}

constexpr DIYActivationCodePageLayout MakeFallbackDIYActivationCodePageLayout_2_9()
{
    return DIYActivationCodePageLayout{
        /*backgroundColor=*/TFT_BLACK,
        /*textColor=*/TFT_WHITE,
        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"Pair Code",
        /*titleAlignment=*/DIYTextAlignment::kLeft,
        /*titleTopPadding=*/5,
        /*titleLeftMargin=*/5,

        /*codeFont=*/u8g2_font_36,
        /*codeAlignment=*/DIYTextAlignment::kLeft,
        /*codeGapBelowTitle=*/15,
        /*codeLeftMargin=*/5,
        /*codeTextColor=*/TFT_WHITE,

        /*infoFont=*/u8g2_font_helvR14_tf,
        /*infoText=*/"Add device on",
        /*infoAlignment=*/DIYTextAlignment::kLeft,
        /*infoGapBelowCode=*/15,
        /*infoLeftMargin=*/5,
        /*infoBottomOffset=*/-1,

        /*urlFont=*/u8g2_font_helvR10_tf,
        /*urlAlignment=*/DIYTextAlignment::kLeft,
        /*urlGapBelowInfo=*/8,
        /*urlLeftMargin=*/5,
        /*urlBottomOffset=*/-1,
        /*urlUnderlineOffset=*/2};
}


constexpr DIYActivationCodePageLayout MakeFallbackDIYActivationCodePageLayout_2_9_bwry()
{
    return DIYActivationCodePageLayout{
        /*backgroundColor=*/TFT_RED,
        /*textColor=*/TFT_WHITE,
        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"Pair Code",
        /*titleAlignment=*/DIYTextAlignment::kLeft,
        /*titleTopPadding=*/5,
        /*titleLeftMargin=*/5,

        /*codeFont=*/u8g2_font_36,
        /*codeAlignment=*/DIYTextAlignment::kLeft,
        /*codeGapBelowTitle=*/15,
        /*codeLeftMargin=*/5,
        /*codeTextColor=*/TFT_YELLOW,

        /*infoFont=*/u8g2_font_helvR14_tf,
        /*infoText=*/"Add device on",
        /*infoAlignment=*/DIYTextAlignment::kLeft,
        /*infoGapBelowCode=*/15,
        /*infoLeftMargin=*/5,
        /*infoBottomOffset=*/-1,

        /*urlFont=*/u8g2_font_helvR10_tf,
        /*urlAlignment=*/DIYTextAlignment::kLeft,
        /*urlGapBelowInfo=*/8,
        /*urlLeftMargin=*/5,
        /*urlBottomOffset=*/-1,
        /*urlUnderlineOffset=*/2};
}



#if (BOARD_SCREEN_COMBO == 505)
constexpr DIYActivationCodePageLayout kDIYActivationCodePageLayout = MakeFallbackDIYActivationCodePageLayout_1_54();
#elif (BOARD_SCREEN_COMBO == 517)
constexpr DIYActivationCodePageLayout kDIYActivationCodePageLayout = MakeFallbackDIYActivationCodePageLayout_1_54();
#elif (BOARD_SCREEN_COMBO == 508)
constexpr DIYActivationCodePageLayout kDIYActivationCodePageLayout = MakeFallbackDIYActivationCodePageLayout_2_13();
#elif (BOARD_SCREEN_COMBO == 513)
constexpr DIYActivationCodePageLayout kDIYActivationCodePageLayout = MakeFallbackDIYActivationCodePageLayout_2_13_bwry();
#elif (BOARD_SCREEN_COMBO == 504)
constexpr DIYActivationCodePageLayout kDIYActivationCodePageLayout = MakeFallbackDIYActivationCodePageLayout_2_9();
#elif (BOARD_SCREEN_COMBO == 512)
constexpr DIYActivationCodePageLayout kDIYActivationCodePageLayout = MakeFallbackDIYActivationCodePageLayout_2_9_bwry();
#elif (BOARD_SCREEN_COMBO == 515)
constexpr DIYActivationCodePageLayout kDIYActivationCodePageLayout = MakeFallbackDIYActivationCodePageLayout();
#elif (BOARD_SCREEN_COMBO == 516)
constexpr DIYActivationCodePageLayout kDIYActivationCodePageLayout = MakeFallbackDIYActivationCodePageLayout();
#else
constexpr DIYActivationCodePageLayout kDIYActivationCodePageLayout = MakeFallbackDIYActivationCodePageLayout();
#endif
