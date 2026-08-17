#ifndef EPD_COLOR_MAP_H
#define EPD_COLOR_MAP_H

#include <stdint.h>
#include <vector>
#include <set>

typedef enum {
    EPD_MODE_BW,        // black & white
    EPD_MODE_GRAY4,     // 4-level gray
    EPD_MODE_COLOR6,    // 6-color (includes 4-color)
    EPD_MODE_GRAY16,    // 16-level gray
    EPD_MODE_UNKNOWN    // fallback / unmapped
} EpdMode;

// Color entry definition
struct EpdColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    int     epd_val;
    
    bool operator<(const EpdColor& other) const {
        if (r != other.r) return r < other.r;
        if (g != other.g) return g < other.g;
        return b < other.b;
    }
    bool operator==(const EpdColor& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

// Black & White
const EpdColor MAP_BW[] = {
    {0,   0,   0,   TFT_BLACK}, // TFT_BLACK
    {255, 255, 255, TFT_WHITE}  // TFT_WHITE
};

// 4-level gray
const EpdColor MAP_GRAY4[] = {
    {0,   0,   0,   0x00}, // TFT_GRAY_0 (Black)
    {85,  85,  85,  0x01}, // TFT_GRAY_1
    {170, 170, 170, 0x02}, // TFT_GRAY_2
    {255, 255, 255, 0x03}  // TFT_GRAY_3 (White)
};

// 6-color
const EpdColor MAP_COLOR6[] = {
    {0,   0,   0,   0x0F}, // TFT_BLACK
    {255, 255, 255, 0x00}, // TFT_WHITE
    {255, 0,   0,   0x06}, // TFT_RED
    {0,   255, 0,   0x02}, // TFT_GREEN
    {0,   0,   255, 0x0D}, // TFT_BLUE
    {255, 255, 0,   0x0B}  // TFT_YELLOW
};

// 4. 16-level gray
const EpdColor MAP_GRAY16[] = {
    {0,   0,   0,   0x00}, // TFT_GRAY_0
    {17,  17,  17,  0x01},
    {34,  34,  34,  0x02},
    {51,  51,  51,  0x03},
    {68,  68,  68,  0x04},
    {85,  85,  85,  0x05},
    {102, 102, 102, 0x06},
    {119, 119, 119, 0x07},
    {136, 136, 136, 0x08},
    {153, 153, 153, 0x09},
    {170, 170, 170, 0x0A},
    {187, 187, 187, 0x0B},
    {204, 204, 204, 0x0C},
    {221, 221, 221, 0x0D},
    {238, 238, 238, 0x0E},
    {255, 255, 255, 0x0F}  // TFT_GRAY_15
};

#endif // EPD_COLOR_MAP_H
