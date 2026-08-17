#pragma once

#include <stdint.h>

#include "driver.h"
#include "resources/pages/diy_layout_common.h"


struct DIYHomePageLayout
{
    uint16_t backgroundColor;

    const uint8_t *titleFont;
    const char *titleText;
    DIYTextAlignment titleAlignment;
    int16_t titleTopPadding;
    int16_t titleLeftMargin;
    uint16_t titleTextColor;

    const uint8_t *subtitleFont;
    const char *subtitleText;
    DIYTextAlignment subtitleAlignment;
    int16_t subtitleLeftMargin;
    int16_t subtitleGapBelowTitle;
    uint16_t subtitleTextColor;

    const uint8_t *indexFont;
    DIYTextAlignment indexAlignment;
    int16_t indexLeftMargin;
    int16_t indexTopPadding;
    int16_t indexBoxWidth;
    int16_t indexBoxHeight;
    int16_t indexBoxCornerRadius;
    uint16_t indexBoxFillColor;
    uint16_t indexBoxBorderColor;
    int16_t indexBoxBorderThickness;
    uint16_t indexTextColor;
    int16_t indexBaselineAdjust;

    const uint8_t *bodyFont;
    const char *bodyPrefix;
    const char *bodySuffix;
    DIYBodyPlacement bodyPlacement;
    DIYTextAlignment bodyAlignment;
    int16_t bodyLeftMargin;
    int16_t bodyGapFromIndex;
    int16_t bodyBottomOffset;
    uint16_t bodyTextColor;
    bool bodyWrapHotspotInQuotes;
};

constexpr DIYHomePageLayout MakeFallbackDIYHomePageLayout()
{
    return DIYHomePageLayout{
        /*backgroundColor=*/TFT_BLACK,

        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"ePaper DIY Kit",
        /*titleAlignment=*/DIYTextAlignment::kCenter,
        /*titleTopPadding=*/28,
        /*titleLeftMargin=*/0,
        /*titleTextColor=*/TFT_WHITE,

        /*subtitleFont=*/nullptr,
        /*subtitleText=*/nullptr,
        /*subtitleAlignment=*/DIYTextAlignment::kCenter,
        /*subtitleLeftMargin=*/0,
        /*subtitleGapBelowTitle=*/8,
        /*subtitleTextColor=*/TFT_WHITE,

        /*indexFont=*/u8g2_font_36,
        /*indexAlignment=*/DIYTextAlignment::kCenter,
        /*indexLeftMargin=*/0,
        /*indexTopPadding=*/88,
        /*indexBoxWidth=*/84,
        /*indexBoxHeight=*/84,
        /*indexBoxCornerRadius=*/10,
        /*indexBoxFillColor=*/TFT_WHITE,
        /*indexBoxBorderColor=*/TFT_WHITE,
        /*indexBoxBorderThickness=*/0,
        /*indexTextColor=*/TFT_BLACK,
        /*indexBaselineAdjust=*/4,

        /*bodyFont=*/u8g2_font_helvR14_tf,
        /*bodyPrefix=*/nullptr,
        /*bodySuffix=*/".",
        /*bodyPlacement=*/DIYBodyPlacement::kBelowIndex,
        /*bodyAlignment=*/DIYTextAlignment::kCenter,
        /*bodyLeftMargin=*/0,
        /*bodyGapFromIndex=*/32,
        /*bodyBottomOffset=*/24,
        /*bodyTextColor=*/TFT_WHITE,
        /*bodyWrapHotspotInQuotes=*/false};
}

constexpr DIYHomePageLayout MakeFallbackDIYHomePageLayout_1_54_mono()
{
    return DIYHomePageLayout{
        /*backgroundColor=*/TFT_BLACK,

        /*titleFont=*/u8g2_font_helvR18_tf,
        /*titleText=*/"ePaper DIY Kit",
        /*titleAlignment=*/DIYTextAlignment::kCenter,
        /*titleTopPadding=*/28,
        /*titleLeftMargin=*/0,
        /*titleTextColor=*/TFT_WHITE,

        /*subtitleFont=*/nullptr,
        /*subtitleText=*/nullptr,
        /*subtitleAlignment=*/DIYTextAlignment::kCenter,
        /*subtitleLeftMargin=*/0,
        /*subtitleGapBelowTitle=*/8,
        /*subtitleTextColor=*/TFT_WHITE,

        /*indexFont=*/u8g2_font_36,
        /*indexAlignment=*/DIYTextAlignment::kCenter,
        /*indexLeftMargin=*/0,
        /*indexTopPadding=*/70,
        /*indexBoxWidth=*/60,
        /*indexBoxHeight=*/60,
        /*indexBoxCornerRadius=*/10,
        /*indexBoxFillColor=*/TFT_WHITE,
        /*indexBoxBorderColor=*/TFT_WHITE,
        /*indexBoxBorderThickness=*/0,
        /*indexTextColor=*/TFT_BLACK,
        /*indexBaselineAdjust=*/0,

        /*bodyFont=*/u8g2_font_helvR14_tf,
        /*bodyPrefix=*/nullptr,
        /*bodySuffix=*/".",
        /*bodyPlacement=*/DIYBodyPlacement::kBelowIndex,
        /*bodyAlignment=*/DIYTextAlignment::kCenter,
        /*bodyLeftMargin=*/0,
        /*bodyGapFromIndex=*/18,
        /*bodyBottomOffset=*/-1,
        /*bodyTextColor=*/TFT_WHITE,
        /*bodyWrapHotspotInQuotes=*/false};
}

constexpr DIYHomePageLayout MakeFallbackDIYHomePageLayout_2_13_mono()
{
    return DIYHomePageLayout{
        /*backgroundColor=*/TFT_BLACK,

        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"ePaper DIY Kit",
        /*titleAlignment=*/DIYTextAlignment::kLeft,
        /*titleTopPadding=*/15,
        /*titleLeftMargin=*/5,
        /*titleTextColor=*/TFT_WHITE,

        /*subtitleFont=*/u8g2_font_helvR12_tf,
        /*subtitleText=*/"2.13 inch",
        /*subtitleAlignment=*/DIYTextAlignment::kCenter,
        /*subtitleLeftMargin=*/5,
        /*subtitleGapBelowTitle=*/10,
        /*subtitleTextColor=*/TFT_WHITE,

        /*indexFont=*/u8g2_font_36,
        /*indexAlignment=*/DIYTextAlignment::kLeft,
        /*indexLeftMargin=*/5,
        /*indexTopPadding=*/67,
        /*indexBoxWidth=*/50,
        /*indexBoxHeight=*/50,
        /*indexBoxCornerRadius=*/8,
        /*indexBoxFillColor=*/TFT_WHITE,
        /*indexBoxBorderColor=*/TFT_WHITE,
        /*indexBoxBorderThickness=*/0,
        /*indexTextColor=*/TFT_BLACK,
        /*indexBaselineAdjust=*/0,

        /*bodyFont=*/u8g2_font_helvR12_tf,
        /*bodyPrefix=*/nullptr,
        /*bodySuffix=*/".",
        /*bodyPlacement=*/DIYBodyPlacement::kRightOfIndex,
        /*bodyAlignment=*/DIYTextAlignment::kLeft,
        /*bodyLeftMargin=*/5,
        /*bodyGapFromIndex=*/5,
        /*bodyBottomOffset=*/-1,
        /*bodyTextColor=*/TFT_WHITE,
        /*bodyWrapHotspotInQuotes=*/false};
}

constexpr DIYHomePageLayout MakeFallbackDIYHomePageLayout_2_13_bwry()
{
    return DIYHomePageLayout{
        /*backgroundColor=*/TFT_RED,

        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"ePaper DIY Kit",
        /*titleAlignment=*/DIYTextAlignment::kLeft,
        /*titleTopPadding=*/15,
        /*titleLeftMargin=*/5,
        /*titleTextColor=*/TFT_WHITE,

        /*subtitleFont=*/u8g2_font_helvR12_tf,
        /*subtitleText=*/"2.13 inch",
        /*subtitleAlignment=*/DIYTextAlignment::kCenter,
        /*subtitleLeftMargin=*/5,
        /*subtitleGapBelowTitle=*/10,
        /*subtitleTextColor=*/TFT_YELLOW,

        /*indexFont=*/u8g2_font_36,
        /*indexAlignment=*/DIYTextAlignment::kLeft,
        /*indexLeftMargin=*/5,
        /*indexTopPadding=*/67,
        /*indexBoxWidth=*/50,
        /*indexBoxHeight=*/50,
        /*indexBoxCornerRadius=*/8,
        /*indexBoxFillColor=*/TFT_YELLOW,
        /*indexBoxBorderColor=*/TFT_WHITE,
        /*indexBoxBorderThickness=*/0,
        /*indexTextColor=*/TFT_BLACK,
        /*indexBaselineAdjust=*/0,

        /*bodyFont=*/u8g2_font_helvR12_tf,
        /*bodyPrefix=*/nullptr,
        /*bodySuffix=*/".",
        /*bodyPlacement=*/DIYBodyPlacement::kRightOfIndex,
        /*bodyAlignment=*/DIYTextAlignment::kLeft,
        /*bodyLeftMargin=*/5,
        /*bodyGapFromIndex=*/5,
        /*bodyBottomOffset=*/-1,
        /*bodyTextColor=*/TFT_WHITE,
        /*bodyWrapHotspotInQuotes=*/false};
}

constexpr DIYHomePageLayout MakeFallbackDIYHomePageLayout_2_9_mono()
{
    return DIYHomePageLayout{
        /*backgroundColor=*/TFT_BLACK,

        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"ePaper DIY Kit",
        /*titleAlignment=*/DIYTextAlignment::kLeft,
        /*titleTopPadding=*/10,
        /*titleLeftMargin=*/5,
        /*titleTextColor=*/TFT_WHITE,

        /*subtitleFont=*/u8g2_font_helvR12_tf,
        /*subtitleText=*/"2.9 inch",
        /*subtitleAlignment=*/DIYTextAlignment::kCenter,
        /*subtitleLeftMargin=*/5,
        /*subtitleGapBelowTitle=*/10,
        /*subtitleTextColor=*/TFT_WHITE,

        /*indexFont=*/u8g2_font_36,
        /*indexAlignment=*/DIYTextAlignment::kLeft,
        /*indexLeftMargin=*/5,
        /*indexTopPadding=*/63,
        /*indexBoxWidth=*/60,
        /*indexBoxHeight=*/60,
        /*indexBoxCornerRadius=*/8,
        /*indexBoxFillColor=*/TFT_WHITE,
        /*indexBoxBorderColor=*/TFT_WHITE,
        /*indexBoxBorderThickness=*/0,
        /*indexTextColor=*/TFT_BLACK,
        /*indexBaselineAdjust=*/0,

        /*bodyFont=*/u8g2_font_helvR14_tf,
        /*bodyPrefix=*/nullptr,
        /*bodySuffix=*/".",
        /*bodyPlacement=*/DIYBodyPlacement::kRightOfIndex,
        /*bodyAlignment=*/DIYTextAlignment::kLeft,
        /*bodyLeftMargin=*/5,
        /*bodyGapFromIndex=*/5,
        /*bodyBottomOffset=*/-1,
        /*bodyTextColor=*/TFT_WHITE,
        /*bodyWrapHotspotInQuotes=*/false};
}


constexpr DIYHomePageLayout MakeFallbackDIYHomePageLayout_2_9_bwry()
{
    return DIYHomePageLayout{
        /*backgroundColor=*/TFT_RED,

        /*titleFont=*/u8g2_font_helvR24_tf,
        /*titleText=*/"ePaper DIY Kit",
        /*titleAlignment=*/DIYTextAlignment::kLeft,
        /*titleTopPadding=*/10,
        /*titleLeftMargin=*/5,
        /*titleTextColor=*/TFT_WHITE,

        /*subtitleFont=*/u8g2_font_helvR12_tf,
        /*subtitleText=*/"2.9 inch",
        /*subtitleAlignment=*/DIYTextAlignment::kCenter,
        /*subtitleLeftMargin=*/5,
        /*subtitleGapBelowTitle=*/10,
        /*subtitleTextColor=*/TFT_YELLOW,

        /*indexFont=*/u8g2_font_36,
        /*indexAlignment=*/DIYTextAlignment::kLeft,
        /*indexLeftMargin=*/5,
        /*indexTopPadding=*/63,
        /*indexBoxWidth=*/60,
        /*indexBoxHeight=*/60,
        /*indexBoxCornerRadius=*/8,
        /*indexBoxFillColor=*/TFT_YELLOW,
        /*indexBoxBorderColor=*/TFT_WHITE,
        /*indexBoxBorderThickness=*/0,
        /*indexTextColor=*/TFT_BLACK,
        /*indexBaselineAdjust=*/0,

        /*bodyFont=*/u8g2_font_helvR14_tf,
        /*bodyPrefix=*/nullptr,
        /*bodySuffix=*/".",
        /*bodyPlacement=*/DIYBodyPlacement::kRightOfIndex,
        /*bodyAlignment=*/DIYTextAlignment::kLeft,
        /*bodyLeftMargin=*/5,
        /*bodyGapFromIndex=*/5,
        /*bodyBottomOffset=*/-1,
        /*bodyTextColor=*/TFT_WHITE,
        /*bodyWrapHotspotInQuotes=*/false};
}

// TODO: provide tuned presets for other panels as they become available.
#if (BOARD_SCREEN_COMBO == 505)
constexpr DIYHomePageLayout kDIYHomePageLayout = MakeFallbackDIYHomePageLayout_1_54_mono();
#elif (BOARD_SCREEN_COMBO == 517)
constexpr DIYHomePageLayout kDIYHomePageLayout = MakeFallbackDIYHomePageLayout_1_54_mono();
#elif (BOARD_SCREEN_COMBO == 508)
constexpr DIYHomePageLayout kDIYHomePageLayout = MakeFallbackDIYHomePageLayout_2_13_mono();
#elif (BOARD_SCREEN_COMBO == 513)
constexpr DIYHomePageLayout kDIYHomePageLayout = MakeFallbackDIYHomePageLayout_2_13_bwry();
#elif (BOARD_SCREEN_COMBO == 504)
constexpr DIYHomePageLayout kDIYHomePageLayout = MakeFallbackDIYHomePageLayout_2_9_mono();
#elif (BOARD_SCREEN_COMBO == 512)
constexpr DIYHomePageLayout kDIYHomePageLayout = MakeFallbackDIYHomePageLayout_2_9_bwry();
#elif (BOARD_SCREEN_COMBO == 515)
constexpr DIYHomePageLayout kDIYHomePageLayout = MakeFallbackDIYHomePageLayout();
#elif (BOARD_SCREEN_COMBO == 516)
constexpr DIYHomePageLayout kDIYHomePageLayout = MakeFallbackDIYHomePageLayout();
#else
constexpr DIYHomePageLayout kDIYHomePageLayout = MakeFallbackDIYHomePageLayout();
#endif
