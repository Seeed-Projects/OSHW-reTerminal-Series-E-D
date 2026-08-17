#pragma once

#if defined(BOARD_XIAO_DIY_EE02) || defined(BOARD_XIAO_DIY_EE03) || defined(BOARD_XIAO_DIY_EE04) || defined(BOARD_XIAO_DIY_EE05)
    #define BOARD_XIAO_DIY_ANY
#endif

#ifdef BOARD_SEEED_RETERMINAL_E1001
    #define BOARD_SCREEN_COMBO 520 // reTerminal E1001 （UC8179）
#elif defined(BOARD_SEEED_RETERMINAL_E1002)
    #define BOARD_SCREEN_COMBO 521 // reTerminal E1002 （ED2208）
#elif defined(BOARD_SEEED_RETERMINAL_E1003)
    #define BOARD_SCREEN_COMBO 522
#elif defined(BOARD_SEEED_RETERMINAL_E1004)
    #define BOARD_SCREEN_COMBO 523
#elif defined(BOARD_XIAO_DIY_ANY)
    // XIAO DIY selection begin
    // Keep exactly one USE_XIAO_EPAPER_DISPLAY_BOARD_* and one BOARD_SCREEN_COMBO active.
    // scripts/batch_build_xiao_diy.py toggles these defaults for local batch builds.

    // #define USE_XIAO_EPAPER_DISPLAY_BOARD_EE02
    // #define BOARD_SCREEN_COMBO 510 // 13.3 inch six-color ePaper Screen（T133A01）

    // #define USE_XIAO_EPAPER_DISPLAY_BOARD_EE03
    // #define BOARD_SCREEN_COMBO 511 // 10.3 inch monochrome ePaper Screen（ED103TC2）

    // #define USE_XIAO_EPAPER_DISPLAY_BOARD_EE04
    #if !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE02) && \
        !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE03) && \
        !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE04) && \
        !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE05)
        #define USE_XIAO_EPAPER_DISPLAY_BOARD_EE05
    #endif

    #ifndef BOARD_SCREEN_COMBO
        #define BOARD_SCREEN_COMBO 505 // 1.54 inch monochrome ePaper Screen （SSD1681）
    #endif
    // #define BOARD_SCREEN_COMBO 517 // 1.54 inch BWRY ePaper Screen （SSD1681）
    // #define BOARD_SCREEN_COMBO 508 // 2.13 inch monochrome ePaper Screen （SSD1680）
    // #define BOARD_SCREEN_COMBO 513 // 2.13 inch BWRY ePaper Screen （JD79676）
    // #define BOARD_SCREEN_COMBO 504 // 2.9 inch monochrome ePaper Screen （SSD1680）
    // #define BOARD_SCREEN_COMBO 512 // 2.9 inch BWRY ePaper Screen （JD79667）
    // #define BOARD_SCREEN_COMBO 506 // 4.26 inch monochrome ePaper Screen （SSD1677）
    // #define BOARD_SCREEN_COMBO 515 // 3.97 inch monochrome ePaper Screen （UC8253）
    // #define BOARD_SCREEN_COMBO 516 // 3.97 inch BWRY ePaper Screen （UC8253）
    // #define BOARD_SCREEN_COMBO 509 // 7.3 inch six-color ePaper Screen（ED2208）
    // #define BOARD_SCREEN_COMBO 502 // 7.5 inch monochrome ePaper Screen （UC8179）
    // XIAO DIY selection end

#elif defined(BOARD_XIAO_EPAPER_DISPLAY)
    #define USE_XIAO_EPAPER_DISPLAY_BOARD_EE04
    #define BOARD_SCREEN_COMBO 502 // 7.5 inch monochrome ePaper Screen （UC8179）
#else
    #error "Unsupported board selection"
#endif

#if defined(BOARD_XIAO_DIY_ANY)
    #if defined(BOARD_SCREEN_COMBO_OVERRIDE)
        #undef BOARD_SCREEN_COMBO
        #define BOARD_SCREEN_COMBO BOARD_SCREEN_COMBO_OVERRIDE
    #else
        #define BOARD_SCREEN_COMBO_OVERRIDE BOARD_SCREEN_COMBO
    #endif
#endif

#if defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE05) && (BOARD_SCREEN_COMBO == 509)
    #error "EE05 does not support the 7.3 inch six-color ePaper Screen (BOARD_SCREEN_COMBO 509)"
#endif
