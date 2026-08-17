#include "utils/image_parser.h"

#include "app_config.h"
#include "hal/hal.h"

#include <set>
#include <vector>
#include <limits>
#include <cstdlib>
#include <cstring>

#include "ArduinoLog.h"
#include "TFT_eSPI.h"
#include "utils/epd_color_map.h"
#include "pngle.h"
#include "epd_decoder.h"
#include "esp_heap_caps.h"

namespace
{
EPaper& display()
{
    return HAL::GetHAL().display();
}

constexpr size_t kPngDecodeChunkSize = 1024 * 4;
constexpr size_t kMaxTrackedColors = 32;

EpdMode g_activeImageMode = EPD_MODE_UNKNOWN;

uint16_t screenCombo()
{
    return HAL::GetHAL().screen().combo_id;
}

bool screenComboIs(uint16_t combo_id)
{
    return screenCombo() == combo_id;
}

bool is_reterminal_e1004()
{
    return HAL::GetHAL().boardProfile().model == board_registry::BoardModel::ReTerminalE1004;
}

uint16_t screen_rotation_map(size_t orientationIndex = 0)
{
    return HAL::GetHAL().screenRotation(orientationIndex);
}

uint16_t read16(File &file)
{
    uint16_t result;
    ((uint8_t *)&result)[0] = file.read();
    ((uint8_t *)&result)[1] = file.read();
    return result;
}

uint32_t read32(File &file)
{
    uint32_t result;
    ((uint8_t *)&result)[0] = file.read();
    ((uint8_t *)&result)[1] = file.read();
    ((uint8_t *)&result)[2] = file.read();
    ((uint8_t *)&result)[3] = file.read();
    return result;
}

bool is_subset(const std::set<EpdColor> &imgColors, const EpdColor *palette, size_t paletteSize)
{
    for (const auto &color : imgColors)
    {
        bool found = false;
        for (size_t i = 0; i < paletteSize; ++i)
        {
            if (color == palette[i])
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

EpdMode detect_image_mode(const std::set<EpdColor> &uniqueColors)
{
    if (is_subset(uniqueColors, MAP_BW, sizeof(MAP_BW) / sizeof(EpdColor)))
    {
        return EPD_MODE_BW;
    }

    if (is_subset(uniqueColors, MAP_COLOR6, sizeof(MAP_COLOR6) / sizeof(EpdColor)))
    {
        return EPD_MODE_COLOR6;
    }

    if (is_subset(uniqueColors, MAP_GRAY4, sizeof(MAP_GRAY4) / sizeof(EpdColor)))
    {
        return EPD_MODE_GRAY4;
    }

    if (is_subset(uniqueColors, MAP_GRAY16, sizeof(MAP_GRAY16) / sizeof(EpdColor)))
    {
        return EPD_MODE_GRAY16;
    }

    return EPD_MODE_UNKNOWN;
}

void apply_image_mode(EpdMode mode)
{
    g_activeImageMode = mode;

#if defined(USE_MUTIGRAY_EPAPER)
    if (mode == EPD_MODE_GRAY4)
    {
#if defined(GRAY_LEVEL4)
        display().initGrayMode(GRAY_LEVEL4);
#else
        display().deinitGrayMode();
#endif
    }
    else if (mode == EPD_MODE_GRAY16)
    {
#if defined(GRAY_LEVEL16)
        display().initGrayMode(GRAY_LEVEL16);
#else
        display().deinitGrayMode();
#endif
    }
    else
    {
#if defined(USE_MUTIGRAY_EPAPER)
        display().deinitGrayMode();
#endif
    }
#endif
}

const char *epd_mode_to_string(EpdMode mode)
{
    switch (mode)
    {
    case EPD_MODE_BW:
        return "Black & White";
    case EPD_MODE_GRAY4:
        return "4-level Grayscale";
    case EPD_MODE_COLOR6:
        return "6-color";
    case EPD_MODE_GRAY16:
        return "16-level Grayscale";
    default:
        return "Unknown";
    }
}

void log_image_color_mode(const std::set<EpdColor> &uniqueColors, const char *imageTag)
{
    if (uniqueColors.empty())
    {
        apply_image_mode(EPD_MODE_UNKNOWN);
        Log.infoln("[image_parser] %s image color type: Unknown (no colors detected)", imageTag);
        return;
    }

    EpdMode mode = detect_image_mode(uniqueColors);
    apply_image_mode(mode);
    Log.infoln("[image_parser] %s image color type: %s", imageTag, epd_mode_to_string(mode));
}

const EpdColor *palette_for_mode(EpdMode mode, size_t &paletteSize)
{
    switch (mode)
    {
    case EPD_MODE_BW:
        paletteSize = sizeof(MAP_BW) / sizeof(EpdColor);
        return MAP_BW;
    case EPD_MODE_GRAY4:
        paletteSize = sizeof(MAP_GRAY4) / sizeof(EpdColor);
        return MAP_GRAY4;
    case EPD_MODE_COLOR6:
        paletteSize = sizeof(MAP_COLOR6) / sizeof(EpdColor);
        return MAP_COLOR6;
    case EPD_MODE_GRAY16:
        paletteSize = sizeof(MAP_GRAY16) / sizeof(EpdColor);
        return MAP_GRAY16;
    default:
        paletteSize = 0;
        return nullptr;
    }
}

uint16_t find_closest_eink_color(uint8_t red, uint8_t green, uint8_t blue)
{
    struct ColorMapping
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint16_t inkColor;
    } colorPalette[] = {
        {0, 0, 0, TFT_BLACK},
        {255, 255, 255, TFT_WHITE},
        {0, 0, 255, TFT_BLUE},
        {255, 255, 0, TFT_YELLOW},
        {0, 255, 0, TFT_GREEN},
        {255, 0, 0, TFT_RED},
    };

    const int numPaletteColors = sizeof(colorPalette) / sizeof(ColorMapping);
    long minDistanceSquared = -1;
    uint16_t closestColor = TFT_WHITE;

    for (int i = 0; i < numPaletteColors; ++i)
    {
        long dr = red - colorPalette[i].r;
        long dg = green - colorPalette[i].g;
        long db = blue - colorPalette[i].b;
        long distanceSquared = dr * dr + dg * dg + db * db;

        if (minDistanceSquared == -1 || distanceSquared < minDistanceSquared)
        {
            minDistanceSquared = distanceSquared;
            closestColor = colorPalette[i].inkColor;
        }
    }

    return closestColor;
}

uint16_t fallback_display_color(bool with_color, uint8_t red, uint8_t green, uint8_t blue)
{
    if (with_color)
    {
        return find_closest_eink_color(red, green, blue);
    }

    bool isWhite = (red > 0xE0) && (green > 0xE0) && (blue > 0xE0);
    return isWhite ? TFT_WHITE : TFT_BLACK;
}

uint16_t convert_color_for_active_mode(bool with_color, uint8_t red, uint8_t green, uint8_t blue)
{
    size_t paletteSize = 0;
    const EpdColor *palette = palette_for_mode(g_activeImageMode, paletteSize);

    if (palette != nullptr && paletteSize > 0)
    {
        uint32_t bestDistance = std::numeric_limits<uint32_t>::max();
        const EpdColor *bestMatch = nullptr;

        for (size_t i = 0; i < paletteSize; ++i)
        {
            int32_t dr = static_cast<int32_t>(red) - palette[i].r;
            int32_t dg = static_cast<int32_t>(green) - palette[i].g;
            int32_t db = static_cast<int32_t>(blue) - palette[i].b;
            uint32_t distance = static_cast<uint32_t>(dr * dr + dg * dg + db * db);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestMatch = &palette[i];
                if (distance == 0)
                {
                    break;
                }
            }
        }

        if (bestMatch != nullptr)
        {
            return static_cast<uint16_t>(bestMatch->epd_val);
        }
    }

    return fallback_display_color(with_color, red, green, blue);
}

struct PngleColorCollectContext
{
    std::set<EpdColor> *colorSet = nullptr;
};

void pngle_color_collect_callback(pngle_t *pngle, uint32_t, uint32_t, uint32_t, uint32_t, const uint8_t rgba[4])
{
    PngleColorCollectContext *ctx = static_cast<PngleColorCollectContext *>(pngle_get_user_data(pngle));
    if (ctx == nullptr || ctx->colorSet == nullptr || rgba[3] < 0x80)
    {
        return;
    }

    if (ctx->colorSet->size() >= kMaxTrackedColors)
    {
        return;
    }

    ctx->colorSet->insert({rgba[0], rgba[1], rgba[2], 0});
}

bool collect_png_colors_from_stream(File &file, std::set<EpdColor> &uniqueColors)
{
    pngle_t *pngle = pngle_new();
    if (pngle == nullptr)
    {
        Log.errorln("[image_parser] Failed to allocate pngle decoder for streaming PNG color scan");
        return false;
    }

    PngleColorCollectContext context;
    context.colorSet = &uniqueColors;
    pngle_set_user_data(pngle, &context);
    pngle_set_draw_callback(pngle, pngle_color_collect_callback);

    uint8_t chunk[kPngDecodeChunkSize];
    bool success = true;

    while (success)
    {
        size_t bytesRead = file.read(chunk, sizeof(chunk));
        if (bytesRead == 0)
        {
            break;
        }

        size_t offset = 0;
        while (offset < bytesRead)
        {
            int fed = pngle_feed(pngle, chunk + offset, bytesRead - offset);
            if (fed < 0)
            {
                Log.error("[image_parser] Streaming PNG color scan failed: ");
                Log.errorln(pngle_error(pngle));
                success = false;
                break;
            }

            if (fed == 0)
            {
                Log.errorln("[image_parser] Streaming PNG color scan stalled");
                success = false;
                break;
            }

            offset += static_cast<size_t>(fed);
        }

        if (!success || uniqueColors.size() >= kMaxTrackedColors)
        {
            break;
        }
    }

    pngle_destroy(pngle);
    return success;
}

bool analyze_png_colors_and_apply_mode_from_stream(File &file, const char *imageTag, const char *errorMessage)
{
    std::set<EpdColor> uniqueColors;
    if (collect_png_colors_from_stream(file, uniqueColors))
    {
        log_image_color_mode(uniqueColors, imageTag);
        return true;
    }

    if (errorMessage != nullptr)
    {
        Log.errorln("%s", errorMessage);
    }
    apply_image_mode(EPD_MODE_UNKNOWN);
    return false;
}

void apply_epd_draw_rotation()
{
    // Keep 1.54" EPD image orientation aligned with the DIY startup pages.
    if (screenComboIs(505) || screenComboIs(517))
    {
        display().setRotation(screen_rotation_map(3));
        display().setViewport(0, 0, display().width(), display().height());
    }
}

struct ImagePlacement
{
    int rotationMode = 0;
    uint16_t drawableW = 0;
    uint16_t drawableH = 0;
    uint16_t destWidth = 0;
    uint16_t destHeight = 0;
};

bool prepare_image_placement(int16_t x, int16_t y, uint16_t srcWidth, uint16_t srcHeight, bool overwrite, ImagePlacement &placement)
{
    display().setRotation(screen_rotation_map(0));
    display().setViewport(0, 0, display().width(), display().height());

    int32_t availableW = display().width() - x;
    int32_t availableH = display().height() - y;

    if (availableW <= 0 || availableH <= 0)
    {
        Log.errorln("[image_parser] No drawable area available on display");
        return false;
    }

    placement.drawableW = static_cast<uint16_t>(availableW);
    placement.drawableH = static_cast<uint16_t>(availableH);

    bool normalFits = (srcWidth <= placement.drawableW) && (srcHeight <= placement.drawableH);
    bool rotatedFits = (srcHeight <= placement.drawableW) && (srcWidth <= placement.drawableH);

    bool rotateImage = false;
    if (!normalFits && rotatedFits)
    {
        rotateImage = true;
    }
    else if (!normalFits && !rotatedFits)
    {
        uint64_t normalOverflow = static_cast<uint64_t>(srcWidth > placement.drawableW ? srcWidth - placement.drawableW : 0) * srcHeight +
                                  static_cast<uint64_t>(srcHeight > placement.drawableH ? srcHeight - placement.drawableH : 0) * srcWidth;
        uint64_t rotatedOverflow = static_cast<uint64_t>(srcHeight > placement.drawableW ? srcHeight - placement.drawableW : 0) * srcWidth +
                                   static_cast<uint64_t>(srcWidth > placement.drawableH ? srcWidth - placement.drawableH : 0) * srcHeight;
        if (rotatedOverflow < normalOverflow)
        {
            rotateImage = true;
        }
    }

    placement.rotationMode = rotateImage ? (is_reterminal_e1004() ? 1 : -1) : 0;
    placement.destWidth = rotateImage ? srcHeight : srcWidth;
    placement.destHeight = rotateImage ? srcWidth : srcHeight;

    const char *rotationLabel = rotateImage ? "rotated" : "native";
    if (screenComboIs(505) || screenComboIs(517))
    {
        display().setRotation(screen_rotation_map(3));
    }

    Log.infoln("display().width: %d, display().height: %d", display().width(), display().height());
    if (rotateImage && screenComboIs(506))
    {
        display().setRotation(screen_rotation_map(0));
    }

    Log.infoln("[image_parser] Image target size: %u x %u (%s)",
               static_cast<unsigned>(placement.destWidth),
               static_cast<unsigned>(placement.destHeight),
               rotationLabel);

    uint16_t fillWidth = placement.destWidth > placement.drawableW ? placement.drawableW : placement.destWidth;
    uint16_t fillHeight = placement.destHeight > placement.drawableH ? placement.drawableH : placement.destHeight;

    if (!overwrite && fillWidth > 0 && fillHeight > 0)
    {
        display().fillRect(x, y, fillWidth, fillHeight, TFT_WHITE);
    }

    return true;
}

bool compute_destination_pixel(const ImagePlacement &placement,
                               int16_t baseX,
                               int16_t baseY,
                               uint16_t srcWidth,
                               uint16_t srcHeight,
                               uint16_t srcRow,
                               uint16_t srcCol,
                               int16_t &destX,
                               int16_t &destY)
{
    if (placement.rotationMode > 0)
    {
        destX = baseX + (srcHeight - 1 - srcRow);
        destY = baseY + srcCol;
    }
    else if (placement.rotationMode < 0)
    {
        destX = baseX + srcRow;
        destY = baseY + (srcWidth - 1 - srcCol);
    }
    else
    {
        destX = baseX + srcCol;
        destY = baseY + srcRow;
    }

    if (destX < 0 || destX >= display().width() || destY < 0 || destY >= display().height())
    {
        return false;
    }

    return true;
}

struct BmpStreamInfo
{
    uint32_t fileSize = 0;
    uint32_t headerSize = 0;
    uint32_t imageOffset = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t depth = 0;
    uint16_t paletteCount = 0;
    uint32_t rowSize = 0;
    bool flip = true;
};

bool parse_bmp_header_from_stream(File &file, BmpStreamInfo &info)
{
    if (!file.seek(0))
    {
        Log.errorln("[image_parser] Failed to seek BMP file start");
        return false;
    }

    if (read16(file) != 0x4D42)
    {
        Log.errorln("[image_parser] BMP signature mismatch");
        return false;
    }

    info.fileSize = file.size();
    read32(file);
    read32(file);
    info.imageOffset = read32(file);
    info.headerSize = read32(file);

    if (info.headerSize < 40)
    {
        Log.errorln("[image_parser] Unsupported BMP header size: %lu", static_cast<unsigned long>(info.headerSize));
        return false;
    }

    int32_t rawWidth = static_cast<int32_t>(read32(file));
    int32_t rawHeight = static_cast<int32_t>(read32(file));
    uint16_t planes = read16(file);
    info.depth = read16(file);
    uint32_t format = read32(file);

    if (planes != 1 || format != 0 || !(info.depth == 4 || info.depth == 8))
    {
        Log.errorln("[image_parser] Unsupported BMP format. Only 4-bit/8-bit uncompressed supported.");
        return false;
    }

    if (rawWidth <= 0 || rawWidth > static_cast<int32_t>(UINT16_MAX))
    {
        Log.errorln("[image_parser] BMP width out of supported range: %ld", static_cast<long>(rawWidth));
        return false;
    }

    if (rawHeight == 0 || std::abs(rawHeight) > static_cast<int32_t>(UINT16_MAX))
    {
        Log.errorln("[image_parser] BMP height out of supported range: %ld", static_cast<long>(rawHeight));
        return false;
    }

    info.flip = (rawHeight > 0);
    if (rawHeight < 0)
    {
        rawHeight = -rawHeight;
    }

    info.width = static_cast<uint16_t>(rawWidth);
    info.height = static_cast<uint16_t>(rawHeight);
    info.paletteCount = static_cast<uint16_t>(1u << info.depth);
    info.rowSize = ((static_cast<uint32_t>(info.width) * info.depth + 31) / 32) * 4;

    uint32_t paletteOffset = 14 + info.headerSize;
    uint64_t paletteEnd = static_cast<uint64_t>(paletteOffset) + static_cast<uint64_t>(info.paletteCount) * 4ull;
    if (paletteEnd > info.fileSize)
    {
        Log.errorln("[image_parser] BMP palette extends beyond file size");
        return false;
    }

    uint64_t dataEnd = static_cast<uint64_t>(info.imageOffset) + static_cast<uint64_t>(info.rowSize) * info.height;
    if (dataEnd > info.fileSize)
    {
        Log.errorln("[image_parser] BMP pixel data extends beyond file size");
        return false;
    }

    if (!file.seek(paletteOffset))
    {
        Log.errorln("[image_parser] Failed to seek BMP palette");
        return false;
    }

    return true;
}

bool load_bmp_palette_from_stream(File &file, const BmpStreamInfo &info, EpdColor *rawPalette)
{
    for (uint16_t i = 0; i < info.paletteCount; ++i)
    {
        uint8_t entry[4];
        if (file.read(entry, sizeof(entry)) != sizeof(entry))
        {
            Log.errorln("[image_parser] Failed to read BMP palette entry %u", static_cast<unsigned>(i));
            return false;
        }
        rawPalette[i] = {entry[2], entry[1], entry[0], 0};
    }
    return true;
}

bool analyze_bmp_colors_and_apply_mode_from_stream(File &file, const BmpStreamInfo &info, const EpdColor *rawPalette)
{
    if (!file.seek(info.imageOffset))
    {
        Log.errorln("[image_parser] Failed to seek BMP pixel data for analysis");
        return false;
    }

    std::vector<uint8_t> rowBuffer(info.rowSize);
    std::set<EpdColor> uniqueColors;

    for (uint16_t rowIndex = 0; rowIndex < info.height && uniqueColors.size() < kMaxTrackedColors; ++rowIndex)
    {
        if (file.read(rowBuffer.data(), info.rowSize) != info.rowSize)
        {
            Log.errorln("[image_parser] Failed to read BMP row during analysis");
            return false;
        }

        for (uint16_t srcCol = 0; srcCol < info.width && uniqueColors.size() < kMaxTrackedColors; ++srcCol)
        {
            uint8_t paletteIndex;
            if (info.depth == 8)
            {
                paletteIndex = rowBuffer[srcCol];
            }
            else
            {
                uint8_t byte = rowBuffer[srcCol / 2];
                paletteIndex = (srcCol % 2 == 0) ? ((byte >> 4) & 0x0F) : (byte & 0x0F);
            }
            uniqueColors.insert(rawPalette[paletteIndex]);
        }
    }

    log_image_color_mode(uniqueColors, "BMP");
    return true;
}

bool draw_bmp_pixels_from_stream(File &file,
                                 const BmpStreamInfo &info,
                                 const ImagePlacement &placement,
                                 int16_t baseX,
                                 int16_t baseY,
                                 const uint16_t *einkPalette,
                                 uint32_t startTime)
{
    if (!file.seek(info.imageOffset))
    {
        Log.errorln("[image_parser] Failed to seek BMP pixel data for drawing");
        return false;
    }

    std::vector<uint8_t> rowBuffer(info.rowSize);

    for (uint16_t rowIndex = 0; rowIndex < info.height; ++rowIndex)
    {
        if (file.read(rowBuffer.data(), info.rowSize) != info.rowSize)
        {
            Log.errorln("[image_parser] Failed to read BMP row during drawing");
            return false;
        }

        uint16_t srcRow = info.flip ? (info.height - 1 - rowIndex) : rowIndex;

        for (uint16_t srcCol = 0; srcCol < info.width; ++srcCol)
        {
            int16_t destX;
            int16_t destY;
            if (!compute_destination_pixel(placement, baseX, baseY, info.width, info.height, srcRow, srcCol, destX, destY))
            {
                continue;
            }

            uint8_t paletteIndex;
            if (info.depth == 8)
            {
                paletteIndex = rowBuffer[srcCol];
            }
            else
            {
                uint8_t byte = rowBuffer[srcCol / 2];
                paletteIndex = (srcCol % 2 == 0) ? ((byte >> 4) & 0x0F) : (byte & 0x0F);
            }

            display().drawPixel(destX, destY, einkPalette[paletteIndex]);
        }

        if ((rowIndex % 20) == 0)
        {
            delay(1);
        }
    }

    Log.infoln("[image_parser] Image loaded in %lu ms", static_cast<unsigned long>(millis() - startTime));
    return true;
}

bool draw_bmp_from_stream(File &file, int16_t x, int16_t y, bool with_color, bool overwrite)
{
    BmpStreamInfo info;
    if (!parse_bmp_header_from_stream(file, info))
    {
        return false;
    }

    ImagePlacement placement;
    if (!prepare_image_placement(x, y, info.width, info.height, overwrite, placement))
    {
        return false;
    }

    EpdColor rawPalette[256];
    if (!load_bmp_palette_from_stream(file, info, rawPalette))
    {
        return false;
    }

    if (!analyze_bmp_colors_and_apply_mode_from_stream(file, info, rawPalette))
    {
        return false;
    }

    uint16_t einkPalette[256];
    for (uint16_t i = 0; i < info.paletteCount; ++i)
    {
        einkPalette[i] = convert_color_for_active_mode(with_color, rawPalette[i].r, rawPalette[i].g, rawPalette[i].b);
    }

    return draw_bmp_pixels_from_stream(file, info, placement, x, y, einkPalette, millis());
}

struct PngleDrawContext
{
    int16_t baseX = 0;
    int16_t baseY = 0;
    bool withColor = false;
    bool overwrite = false;
    bool placementReady = false;
    bool abort = false;
    uint16_t srcWidth = 0;
    uint16_t srcHeight = 0;
    ImagePlacement placement;
    int32_t lastDelayRow = -1;
};

uint16_t convert_rgb_to_display_color(bool with_color, uint8_t red, uint8_t green, uint8_t blue)
{
    return convert_color_for_active_mode(with_color, red, green, blue);
}

void pngle_init_callback(pngle_t *pngle, uint32_t width, uint32_t height)
{
    PngleDrawContext *ctx = static_cast<PngleDrawContext *>(pngle_get_user_data(pngle));
    if (ctx == nullptr || ctx->abort)
    {
        return;
    }

    if (width == 0 || height == 0 || width > UINT16_MAX || height > UINT16_MAX)
    {
        Log.infoln("[image_parser] PNG has invalid dimensions");
        ctx->abort = true;
        return;
    }

    ctx->srcWidth = static_cast<uint16_t>(width);
    ctx->srcHeight = static_cast<uint16_t>(height);

    Log.infoln("[image_parser] PNG size: %u x %u",
               static_cast<unsigned>(ctx->srcWidth),
               static_cast<unsigned>(ctx->srcHeight));

    if (!prepare_image_placement(ctx->baseX, ctx->baseY, ctx->srcWidth, ctx->srcHeight, ctx->overwrite, ctx->placement))
    {
        ctx->abort = true;
        return;
    }

    ctx->placementReady = true;
}

void pngle_draw_callback(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4])
{
    PngleDrawContext *ctx = static_cast<PngleDrawContext *>(pngle_get_user_data(pngle));
    if (ctx == nullptr || ctx->abort || !ctx->placementReady || rgba[3] < 0x80)
    {
        return;
    }

    uint16_t color = convert_rgb_to_display_color(ctx->withColor, rgba[0], rgba[1], rgba[2]);
    uint32_t maxRow = y + h;
    uint32_t maxCol = x + w;

    for (uint32_t srcRow = y; srcRow < maxRow && srcRow < ctx->srcHeight; ++srcRow)
    {
        for (uint32_t srcCol = x; srcCol < maxCol && srcCol < ctx->srcWidth; ++srcCol)
        {
            int16_t destX;
            int16_t destY;
            if (!compute_destination_pixel(ctx->placement,
                                           ctx->baseX,
                                           ctx->baseY,
                                           ctx->srcWidth,
                                           ctx->srcHeight,
                                           static_cast<uint16_t>(srcRow),
                                           static_cast<uint16_t>(srcCol),
                                           destX,
                                           destY))
            {
                continue;
            }

            display().drawPixel(destX, destY, color);
        }

        if ((srcRow % 20) == 0 && static_cast<int32_t>(srcRow) != ctx->lastDelayRow)
        {
            delay(1);
            ctx->lastDelayRow = static_cast<int32_t>(srcRow);
        }
    }
}

bool draw_png_from_stream(File &file, int16_t x, int16_t y, bool with_color, bool overwrite, uint32_t startTime)
{
    pngle_t *pngle = pngle_new();
    if (pngle == nullptr)
    {
        Log.errorln("[image_parser] Failed to allocate pngle decoder");
        return false;
    }

    PngleDrawContext context;
    context.baseX = x;
    context.baseY = y;
    context.withColor = with_color;
    context.overwrite = overwrite;

    pngle_set_user_data(pngle, &context);
    pngle_set_init_callback(pngle, pngle_init_callback);
    pngle_set_draw_callback(pngle, pngle_draw_callback);

    uint8_t chunk[kPngDecodeChunkSize];
    bool success = true;

    while (success)
    {
        size_t bytesRead = file.read(chunk, sizeof(chunk));
        if (bytesRead == 0)
        {
            break;
        }

        size_t offset = 0;
        while (offset < bytesRead)
        {
            int fed = pngle_feed(pngle, chunk + offset, bytesRead - offset);
            if (fed < 0)
            {
                Log.error("[image_parser] PNG decode failed while streaming: ");
                Log.errorln(pngle_error(pngle));
                success = false;
                break;
            }

            if (fed == 0)
            {
                Log.errorln("[image_parser] PNG decoder stalled while streaming");
                success = false;
                break;
            }

            offset += static_cast<size_t>(fed);
        }

        if (!success || context.abort)
        {
            break;
        }
    }

    if (success && !context.placementReady)
    {
        success = false;
    }

    if (success)
    {
        Log.infoln("[image_parser] Image loaded in %lu ms", static_cast<unsigned long>(millis() - startTime));
    }

    pngle_destroy(pngle);
    return success;
}

size_t epd_row_bytes(uint32_t width, uint8_t bitDepth)
{
    return ((static_cast<size_t>(width) * bitDepth) + 7) / 8;
}

uint8_t epd_frame_get_pixel(const uint8_t *buffer, size_t rowBytes, uint8_t bitDepth, uint32_t x, uint32_t y)
{
    const uint8_t *rowPtr = buffer + y * rowBytes;
    if (bitDepth == 1)
    {
        size_t byteIndex = x >> 3;
        uint8_t mask = static_cast<uint8_t>(0x80u >> (x & 0x7));
        return (rowPtr[byteIndex] & mask) ? 1 : 0;
    }

    size_t byteIndex = x >> 1;
    uint8_t byte = rowPtr[byteIndex];
    if ((x & 1u) == 0)
    {
        return static_cast<uint8_t>((byte >> 4) & 0x0F);
    }
    return static_cast<uint8_t>(byte & 0x0F);
}

void epd_frame_set_pixel(uint8_t *buffer, size_t rowBytes, uint8_t bitDepth, uint32_t x, uint32_t y, uint8_t value)
{
    uint8_t *rowPtr = buffer + y * rowBytes;
    if (bitDepth == 1)
    {
        size_t byteIndex = x >> 3;
        uint8_t mask = static_cast<uint8_t>(0x80u >> (x & 0x7));
        if (value)
        {
            rowPtr[byteIndex] |= mask;
        }
        else
        {
            rowPtr[byteIndex] &= static_cast<uint8_t>(~mask);
        }
        return;
    }

    size_t byteIndex = x >> 1;
    uint8_t nibble = static_cast<uint8_t>(value & 0x0F);
    if ((x & 1u) == 0)
    {
        rowPtr[byteIndex] = static_cast<uint8_t>((rowPtr[byteIndex] & 0x0F) | (nibble << 4));
    }
    else
    {
        rowPtr[byteIndex] = static_cast<uint8_t>((rowPtr[byteIndex] & 0xF0) | nibble);
    }
}

bool rotate_epd_frame(EpdFrameBuffer &frame, bool reverse_direction)
{
    uint32_t newWidth = frame.height;
    uint32_t newHeight = frame.width;
    size_t srcRowBytes = epd_row_bytes(frame.width, frame.bitDepth);
    size_t dstRowBytes = epd_row_bytes(newWidth, frame.bitDepth);
    size_t rotatedSize = dstRowBytes * newHeight;

    uint8_t *rotatedPixels = static_cast<uint8_t *>(heap_caps_malloc(rotatedSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (rotatedPixels == nullptr)
    {
        Log.errorln("[image_parser] Failed to allocate %u bytes for rotated EPD frame", static_cast<unsigned>(rotatedSize));
        return false;
    }
    memset(rotatedPixels, 0, rotatedSize);

    for (uint32_t srcY = 0; srcY < frame.height; ++srcY)
    {
        for (uint32_t srcX = 0; srcX < frame.width; ++srcX)
        {
            uint8_t value = epd_frame_get_pixel(frame.pixels, srcRowBytes, frame.bitDepth, srcX, srcY);
            uint32_t destX = reverse_direction ? (frame.height - 1 - srcY) : srcY;
            uint32_t destY = reverse_direction ? srcX : (frame.width - 1 - srcX);
            epd_frame_set_pixel(rotatedPixels, dstRowBytes, frame.bitDepth, destX, destY, value);
        }
        if ((srcY & 0x1Fu) == 0)
        {
            delay(1);
        }
    }

    heap_caps_free(frame.pixels);
    frame.pixels = rotatedPixels;
    frame.width = newWidth;
    frame.height = newHeight;
    frame.dataSize = rotatedSize;
    return true;
}

bool draw_epd_frame_internal(File &file)
{
    if (!file.seek(0))
    {
        Log.errorln("[image_parser] Failed to seek EPD file");
        return false;
    }

    uint8_t header[16];
    if (file.read(header, sizeof(header)) != sizeof(header))
    {
        Log.errorln("[image_parser] Failed to read EPD header");
        return false;
    }

    uint8_t bitDepth = header[5];
    uint8_t modeVal = header[6];

    EpdMode targetMode = EPD_MODE_UNKNOWN;
    if (bitDepth == 1)
    {
        targetMode = EPD_MODE_BW;
    }
    else if (bitDepth == 4)
    {
        if (modeVal >= EPD_MODE_GRAY4 && modeVal <= EPD_MODE_GRAY16)
        {
            targetMode = static_cast<EpdMode>(modeVal);
        }
        else
        {
            targetMode = EPD_MODE_GRAY16;
            Log.infoln("[image_parser] Legacy EPD file (Mode=0), defaulting to Gray16");
        }
    }

    apply_image_mode(targetMode);
    Log.infoln("[image_parser] EPD mode set to: %s (%d)", epd_mode_to_string(targetMode), targetMode);

    if (!file.seek(0))
    {
        Log.errorln("[image_parser] Failed to rewind EPD file");
        return false;
    }

    uint32_t decodeStart = millis();
    EpdFrameBuffer frame;
    bool decoded = epd_decode_frame(file, frame);
    uint32_t decodeElapsed = millis() - decodeStart;

    if (!decoded)
    {
        return false;
    }

    Log.infoln("[image_parser] EPD frame decoded in %lu ms (w=%lu h=%lu depth=%u)",
               static_cast<unsigned long>(decodeElapsed),
               static_cast<unsigned long>(frame.width),
               static_cast<unsigned long>(frame.height),
               static_cast<unsigned>(frame.bitDepth));

    display().setRotation(screen_rotation_map(0));
    display().setViewport(0, 0, display().width(), display().height());
    uint32_t targetWidth = static_cast<uint32_t>(display().width());
    uint32_t targetHeight = static_cast<uint32_t>(display().height());

    if (frame.width > targetWidth || frame.height > targetHeight)
    {
        bool rotatedFits = (frame.height <= targetWidth) && (frame.width <= targetHeight);
        if (rotatedFits)
        {
            Log.infoln("[image_parser] Rotating EPD frame from %lux%lu to %lux%lu to match display",
                       static_cast<unsigned long>(frame.width),
                       static_cast<unsigned long>(frame.height),
                       static_cast<unsigned long>(frame.height),
                       static_cast<unsigned long>(frame.width));
            if (!rotate_epd_frame(frame, is_reterminal_e1004()))
            {
                epd_free_frame(frame);
                return false;
            }
        }
        else
        {
            Log.errorln("[image_parser] EPD frame exceeds display size");
            epd_free_frame(frame);
            return false;
        }
    }

    uint32_t drawStart = millis();
    apply_epd_draw_rotation();

    if (frame.bitDepth == 4)
    {
        display().fillSprite(TFT_WHITE);
        display().pushImage(0, 0, frame.width, frame.height, reinterpret_cast<uint16_t *>(frame.pixels));
    }
    else
    {
        display().fillSprite(TFT_WHITE);
        display().drawBitmap(0, 0, reinterpret_cast<uint8_t *>(frame.pixels), frame.width, frame.height, TFT_WHITE, TFT_BLACK);
    }

    Log.infoln("[image_parser] EPD frame pushed in %lu ms", static_cast<unsigned long>(millis() - drawStart));

    epd_free_frame(frame);
    return true;
}
} // namespace

namespace image_parser
{
bool draw_from_stream(File &file, int16_t x, int16_t y, bool with_color, bool overwrite)
{
    apply_image_mode(EPD_MODE_UNKNOWN);

    if (!file.seek(0))
    {
        Log.errorln("[image_parser] Failed to rewind file before parsing");
        return false;
    }

    const uint8_t pngSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    uint8_t header[8];
    size_t headerLength = file.read(header, sizeof(header));
    if (headerLength < 4)
    {
        Log.errorln("[image_parser] Failed to read file header");
        return false;
    }

    bool isPng = (headerLength >= sizeof(pngSignature)) && (memcmp(header, pngSignature, sizeof(pngSignature)) == 0);
    bool isBmp = (header[0] == 'B' && header[1] == 'M');
    bool isEpd = (header[0] == 'E' && header[1] == 'P' && header[2] == 'D' && header[3] == '0');

    if (!isPng && !isBmp && !isEpd)
    {
        Log.infoln("[image_parser] Streaming decode supports PNG/BMP/EPD only");
        return false;
    }

    if (!file.seek(0))
    {
        Log.errorln("[image_parser] Failed to rewind file before decode");
        return false;
    }

    if (isEpd)
    {
        return draw_epd_frame_internal(file);
    }

    if (isPng)
    {
        analyze_png_colors_and_apply_mode_from_stream(file,
                                                      "PNG",
                                                      "[image_parser] Failed to analyze streaming PNG colors before drawing");

        if (!file.seek(0))
        {
            Log.errorln("[image_parser] Failed to rewind PNG stream before drawing");
            return false;
        }

        return draw_png_from_stream(file, x, y, with_color, overwrite, millis());
    }

    return draw_bmp_from_stream(file, x, y, with_color, overwrite);
}
} // namespace image_parser
