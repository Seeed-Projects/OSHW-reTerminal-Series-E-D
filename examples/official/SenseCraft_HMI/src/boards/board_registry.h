#ifndef BOARD_REGISTRY_H
#define BOARD_REGISTRY_H

#include <stdint.h>
#include <stddef.h>

#ifndef BOARD_SCREEN_COMBO
#error "BOARD_SCREEN_COMBO must be defined before including board_registry.h"
#endif

namespace board_registry
{
    enum class BoardModel
    {
        ReTerminalE1001,
        ReTerminalE1002,
        ReTerminalE1003,
        ReTerminalE1004,
        XiaoDiyEE02,
        XiaoDiyEE03,
        XiaoDiyEE04,
        XiaoDiyEE05,
        XiaoEpaperDisplay,
        Unknown
    };

    enum class ScreenColor
    {
        Mono,
        Chromatic
    };

    struct BoardProfile
    {
        BoardModel model;
        const char *device_family;
        const char *board_key;
        const char *display_name;
        const char *ble_short_name;
        const char *ap_prefix;
        uint16_t default_screen_combo;
    };

    struct BoardScreenEntry
    {
        BoardModel model;
        uint16_t combo_id;
        const char *board_info_type;
        const char *board_info_screen_type;
        const char *resolution;
        uint16_t width;
        uint16_t height;
        ScreenColor color;
        uint8_t rotation_map[4];
    };

    namespace detail
    {
        constexpr BoardProfile kBoardProfiles[] = {
            {BoardModel::ReTerminalE1001, "reTerminal", "e1001", "reTerminal E1001", "E1001", "reTerminal E1001", 520},
            {BoardModel::ReTerminalE1002, "reTerminal", "e1002", "reTerminal E1002", "E1002", "reTerminal E1002", 521},
            {BoardModel::ReTerminalE1003, "reTerminal", "e1003", "reTerminal E1003", "E1003", "reTerminal E1003", 522},
            {BoardModel::ReTerminalE1004, "reTerminal", "e1004", "reTerminal E1004", "E1004", "reTerminal E1004", 523},
            {BoardModel::XiaoDiyEE02, "xiao_diy_kit", "ee02", "ePaper DIY Kit", "DIY KIT", "ePaper DIY Kit", 510},
            {BoardModel::XiaoDiyEE03, "xiao_diy_kit", "ee03", "ePaper DIY Kit", "DIY KIT", "ePaper DIY Kit", 511},
            {BoardModel::XiaoDiyEE04, "xiao_diy_kit", "ee04", "ePaper DIY Kit", "DIY KIT", "ePaper DIY Kit", 508},
            {BoardModel::XiaoDiyEE05, "xiao_diy_kit", "ee05", "ePaper DIY Kit", "DIY KIT", "ePaper DIY Kit", 508},
            {BoardModel::XiaoEpaperDisplay, "trmnl_diy_kit", "ee04", "ePaper DIY Kit", "DIY KIT", "ePaper DIY Kit", 502},
            {BoardModel::Unknown, "unknown", "unknown", "Unknown Board", "DIY KIT", "DIY KIT", 520}};

        constexpr size_t kBoardProfileCount = sizeof(kBoardProfiles) / sizeof(kBoardProfiles[0]);

        constexpr BoardScreenEntry kBoardScreenTable[] = {
            {BoardModel::ReTerminalE1001, 520, "reterminal_e1001", "7_5_gray4_800_480", "800x480", 800, 480, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::ReTerminalE1002, 521, "reterminal_e1002", "7_3_color_800_480", "800x480", 800, 480, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::ReTerminalE1003, 522, "reterminal_e1003", "10_3_gray16_1872_1404", "1872x1404", 1872, 1404, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::ReTerminalE1004, 523, "reterminal_e1004", "13_3_color_1200_1600", "1200x1600", 1200, 1600, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 505, "xiao_diy_ee04", "1_54_gray4_200_200", "200x200", 200, 200, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 508, "xiao_diy_ee04", "2_13_gray4_250_122", "250x122", 250, 122, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 517, "xiao_diy_ee04", "1_54_bwry_200_200", "200x200", 200, 200, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 513, "xiao_diy_ee04", "2_13_bwry_250_122", "250x122", 250, 122, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 504, "xiao_diy_ee04", "2_9_gray4_296_128", "296x128", 296, 128, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 512, "xiao_diy_ee04", "2_9_bwry_296_128", "296x128", 296, 128, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 506, "xiao_diy_ee04", "4_26_mono_800_480", "800x480", 800, 480, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 515, "xiao_diy_ee04", "3_97_mono_800_480", "800x480", 800, 480, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 516, "xiao_diy_ee04", "3_97_bwry_800_480", "800x480", 800, 480, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 509, "xiao_diy_ee04", "7_3_color_800_480", "800x480", 800, 480, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE04, 502, "xiao_diy_ee04", "7_5_gray4_800_480", "800x480", 800, 480, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 505, "xiao_diy_ee05", "1_54_gray4_200_200", "200x200", 200, 200, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 517, "xiao_diy_ee05", "1_54_bwry_200_200", "200x200", 200, 200, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 508, "xiao_diy_ee05", "2_13_gray4_250_122", "250x122", 250, 122, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 513, "xiao_diy_ee05", "2_13_bwry_250_122", "250x122", 250, 122, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 504, "xiao_diy_ee05", "2_9_gray4_296_128", "296x128", 296, 128, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 512, "xiao_diy_ee05", "2_9_bwry_296_128", "296x128", 296, 128, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 506, "xiao_diy_ee05", "4_26_mono_800_480", "800x480", 800, 480, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 515, "xiao_diy_ee05", "3_97_mono_800_480", "800x480", 800, 480, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 516, "xiao_diy_ee05", "3_97_bwry_800_480", "800x480", 800, 480, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE05, 502, "xiao_diy_ee05", "7_5_gray4_800_480", "800x480", 800, 480, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE03, 511, "xiao_diy_ee03", "10_3_gray16_1872_1404", "1872x1404", 1872, 1404, ScreenColor::Mono, {0, 1, 2, 3}},
            {BoardModel::XiaoDiyEE02, 510, "xiao_diy_ee02", "13_3_color_1200_1600", "1200x1600", 1200, 1600, ScreenColor::Chromatic, {0, 1, 2, 3}},
            {BoardModel::XiaoEpaperDisplay, 502, "trmnl_diy_kit", "7_5_mono_800_480", "800x480", 800, 480, ScreenColor::Mono, {0, 1, 2, 3}}};

        constexpr size_t kBoardScreenCount = sizeof(kBoardScreenTable) / sizeof(kBoardScreenTable[0]);

#if defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE02)
        constexpr BoardModel kDetectedModel = BoardModel::XiaoDiyEE02;
#elif defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE03)
        constexpr BoardModel kDetectedModel = BoardModel::XiaoDiyEE03;
#elif defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE04)
        constexpr BoardModel kDetectedModel = BoardModel::XiaoDiyEE04;
#elif defined(USE_XIAO_EPAPER_DISPLAY_BOARD_EE05)
        constexpr BoardModel kDetectedModel = BoardModel::XiaoDiyEE05;
#elif defined(BOARD_SEEED_RETERMINAL_E1001)
        constexpr BoardModel kDetectedModel = BoardModel::ReTerminalE1001;
#elif defined(BOARD_SEEED_RETERMINAL_E1002)
        constexpr BoardModel kDetectedModel = BoardModel::ReTerminalE1002;
#elif defined(BOARD_SEEED_RETERMINAL_E1003)
        constexpr BoardModel kDetectedModel = BoardModel::ReTerminalE1003;
#elif defined(BOARD_SEEED_RETERMINAL_E1004)
        constexpr BoardModel kDetectedModel = BoardModel::ReTerminalE1004;
#elif defined(BOARD_XIAO_DIY_EE04)
        constexpr BoardModel kDetectedModel = BoardModel::XiaoDiyEE04;
#elif defined(BOARD_XIAO_DIY_EE05)
        constexpr BoardModel kDetectedModel = BoardModel::XiaoDiyEE05;
#elif defined(BOARD_XIAO_EPAPER_DISPLAY)
        constexpr BoardModel kDetectedModel = BoardModel::XiaoEpaperDisplay;
#else
        constexpr BoardModel kDetectedModel = BoardModel::Unknown;
#endif

        static_assert(kDetectedModel != BoardModel::Unknown, "Unsupported board configuration");

        constexpr BoardScreenEntry kUnknownScreen{BoardModel::Unknown, 0, "unknown", "unknown", "0x0", 0, 0, ScreenColor::Mono, {0, 1, 2, 3}};

        inline const BoardProfile &fallback_board_profile()
        {
            return kBoardProfiles[kBoardProfileCount - 1];
        }
    } // namespace detail

    inline const BoardProfile &current_board()
    {
        for (size_t i = 0; i < detail::kBoardProfileCount; ++i)
        {
            if (detail::kBoardProfiles[i].model == detail::kDetectedModel)
            {
                return detail::kBoardProfiles[i];
            }
        }
        return detail::fallback_board_profile();
    }

    inline const BoardProfile &profile(BoardModel model)
    {
        for (size_t i = 0; i < detail::kBoardProfileCount; ++i)
        {
            if (detail::kBoardProfiles[i].model == model)
            {
                return detail::kBoardProfiles[i];
            }
        }
        return detail::fallback_board_profile();
    }

    inline const BoardScreenEntry &board_screen(BoardModel model, uint16_t combo_id)
    {
        for (size_t i = 0; i < detail::kBoardScreenCount; ++i)
        {
            const auto &entry = detail::kBoardScreenTable[i];
            if (entry.model == model && entry.combo_id == combo_id)
            {
                return entry;
            }
        }
        for (size_t i = 0; i < detail::kBoardScreenCount; ++i)
        {
            const auto &entry = detail::kBoardScreenTable[i];
            if (entry.combo_id == combo_id)
            {
                return entry;
            }
        }
        return detail::kUnknownScreen;
    }

    inline const BoardScreenEntry &screen_by_combo(uint16_t combo_id)
    {
        return board_screen(detail::kDetectedModel, combo_id);
    }

    inline const BoardScreenEntry &current_board_screen()
    {
        return board_screen(detail::kDetectedModel, static_cast<uint16_t>(BOARD_SCREEN_COMBO));
    }

    inline bool combo_is_color(uint16_t combo_id)
    {
        const auto &entry = board_screen(detail::kDetectedModel, combo_id);
        return entry.color == ScreenColor::Chromatic;
    }

    inline bool current_screen_is_color()
    {
        return current_board_screen().color == ScreenColor::Chromatic;
    }

    inline bool combo_uses_down_orientation(uint16_t combo_id)
    {
        switch (combo_id)
        {
        case 505: // 1.54 mono
        case 517: // 1.54 BWRY
        case 508: // 2.13 mono
        case 513: // 2.13 BWRY
        case 504: // 2.9 mono
        case 512: // 2.9 BWRY
        case 515: // 3.97 mono
        case 516: // 3.97 BWRY
        case 506: // 4.26 mono
            return true;
        default:
            return false;
        }
    }

    inline uint8_t combo_orientation_index(uint16_t combo_id)
    {
        // Coordinate direction indices: left=0, up=1, right=2, down=3.
        return combo_uses_down_orientation(combo_id) ? 3 : 0;
    }

    inline uint8_t rotation_for_combo(uint16_t combo_id, size_t orientation_index = 0)
    {
        const auto &entry = board_screen(detail::kDetectedModel, combo_id);
        if (orientation_index >= 4)
        {
            orientation_index = 0;
        }
        return entry.rotation_map[orientation_index];
    }

    inline uint8_t current_screen_rotation(size_t orientation_index = 0)
    {
        return rotation_for_combo(static_cast<uint16_t>(BOARD_SCREEN_COMBO), orientation_index);
    }

    inline const char *ble_short_name()
    {
        return current_board().ble_short_name;
    }

    inline const char *ap_prefix()
    {
        return current_board().ap_prefix;
    }
} // namespace board_registry

#endif // BOARD_REGISTRY_H
