#ifndef XIAO_EPAPER_HELLO_DRIVER_H
#define XIAO_EPAPER_HELLO_DRIVER_H

// Seeed_GFX reads this file while compiling its display driver sources.
// Each board + panel combination is selected by two macros, normally
// injected by the CI build flags. The defaults below make the sketch
// compile out of the box in the Arduino IDE (EE04 board + 7.5" panel).
// Seeed_GFX 编译显示驱动源码时会读取本文件。
// 每个「板 + 屏」组合由两个宏决定，正常情况下由 CI 构建参数注入。
// 下方默认值让本工程在 Arduino IDE 里开箱即编译（EE04 板 + 7.5 寸屏）。

// Panel selection: Seeed_GFX setup id (see User_Setups/Dynamic_Setup.h).
// 屏幕选择：Seeed_GFX 配置编号（见 User_Setups/Dynamic_Setup.h）。
//   502 = 7.5"  mono 800x480   (UC8179)
//   503 = 5.83" mono 648x480   (UC8179)
//   504 = 2.9"  mono 296x128   (SSD1680)
//   505 = 1.54" mono 200x200   (SSD1681)
//   506 = 4.26" mono 800x480   (SSD1677)
//   507 = 4.2"  mono 400x300   (SSD1683)
//   508 = 2.13" mono 250x122   (SSD1680)
//   509 = 7.3"  Spectra 6 800x480    (ED2208)
//   510 = 13.3" Spectra 6 1200x1600  (T133A01, EE02 only)
//   511 = 10.3" mono 1872x1404       (ED103TC2 TCON, EE03 only)
//   512 = 2.9"  4-color 296x128  (JD79667)
//   513 = 2.13" 4-color 250x122  (JD79676)
#ifndef BOARD_SCREEN_COMBO
#define BOARD_SCREEN_COMBO 502
#endif

// Board selection: pin map from User_Setups/EPaper_Board_Pins_Setups.h.
// 驱动板选择：引脚映射见 User_Setups/EPaper_Board_Pins_Setups.h。
#if !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE02) && \
    !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE03) && \
    !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE04) && \
    !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE05) && \
    !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EN04) && \
    !defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EN05)
#define USE_XIAO_EPAPER_DISPLAY_BOARD_EE04
#endif

// Human-readable board label rendered on the hello screen.
// 在欢迎画面上显示的驱动板名称。
#if defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE02)
#define HELLO_BOARD_NAME "EE02"
#elif defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE03)
#define HELLO_BOARD_NAME "EE03"
#elif defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE04)
#define HELLO_BOARD_NAME "EE04"
#elif defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE05)
#define HELLO_BOARD_NAME "EE05"
#elif defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EN04)
#define HELLO_BOARD_NAME "EN04"
#elif defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EN05)
#define HELLO_BOARD_NAME "EN05"
#endif

#endif
