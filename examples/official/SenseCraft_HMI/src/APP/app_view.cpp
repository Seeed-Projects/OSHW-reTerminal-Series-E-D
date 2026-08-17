#include "APP/app_view.h"
#include "APP/app_device_info.h"
#include "APP/app_wifi.h"
#include "APP/app_sensecraft.h"
#include "APP/app_input.h"
#include "APP/app_gallery.h"
#include "APP/app_power_manager.h"
#include "APP/app_user_action.h"
#include "boards/common/screen_assets.h"
#include "app_config.h"
#include "hal/hal.h"
#include "utils/image_parser.h"

#include <LittleFS.h>
#include <vector>
#include <algorithm>
#include "WiFi.h"
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "ArduinoLog.h"
#include "U8g2_for_TFT_eSPI.h"

#include "resources/resources.h"
#include "resources/pages/activation_code_layout.h"
#include "resources/pages/home_layout.h"
#include "resources/pages/diy_activation_code_layout.h"
#include "resources/pages/diy_home_layout.h"
#include "TFT_eSPI.h"
#include "minirt.h"
#include "APP/app_events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "OpenFontRender.h"

ESP_EVENT_DEFINE_BASE(VIEW_EVENT_BASE);

static U8g2_for_TFT_eSPI u8g2Fonts;
static OpenFontRender render;

static SemaphoreHandle_t imageBufferMutex = NULL;

#define VIEW_EVT_SHOW_ACTIVATION (1 << 0)
#define VIEW_EVT_SHOW_STARTUP (1 << 1)
#define VIEW_EVT_SHOW_WAIT (1 << 2)
#define VIEW_EVT_SHOW_IMAGE (1 << 3)
#define VIEW_EVT_SHOW_DOWNLOAD (1 << 4)
#define VIEW_EVT_SHOW_CLEAR (1 << 5)
#define VIEW_EVT_SHOW_FAST (1 << 6)
#define VIEW_EVT_SHOW_ERROR (1 << 7)
#define VIEW_EVT_SHOW_LOW_BATTERY (1 << 8)

static constexpr EventBits_t VIEW_EVT_DISPLAY_POWER_MASK =
    VIEW_EVT_SHOW_ACTIVATION |
    VIEW_EVT_SHOW_STARTUP |
    VIEW_EVT_SHOW_WAIT |
    VIEW_EVT_SHOW_IMAGE |
    VIEW_EVT_SHOW_FAST |
    VIEW_EVT_SHOW_CLEAR |
    VIEW_EVT_SHOW_ERROR |
    VIEW_EVT_SHOW_LOW_BATTERY;

static constexpr EventBits_t VIEW_EVT_REFRESH_STATE_MASK =
    VIEW_EVT_SHOW_ACTIVATION |
    VIEW_EVT_SHOW_STARTUP |
    VIEW_EVT_SHOW_WAIT |
    VIEW_EVT_SHOW_IMAGE |
    VIEW_EVT_SHOW_FAST |
    VIEW_EVT_SHOW_ERROR |
    VIEW_EVT_SHOW_LOW_BATTERY;

static EventGroupHandle_t viewEventGroup = NULL;
static ActivationEventData g_savedActivationData;
static SemaphoreHandle_t g_powerMutex = xSemaphoreCreateMutex();
static bool g_powerHeld = false;
static uint32_t g_powerSerial = 0;
static bool g_refreshing = false;
static uint32_t g_refreshSerial = 0;
static char g_savedApName[32];
static uint32_t g_last_gallery_click_ms = 0;
static constexpr uint32_t kGalleryClickGuardMs = 250;

static char g_errorMessage[64] = {0};

static bool app_view_request_gallery_refresh(app_input_event_id_t event_id);

namespace
{
EPaper& display()
{
    return HAL::GetHAL().display();
}

uint16_t screenCombo()
{
    return HAL::GetHAL().screen().combo_id;
}

uint16_t screen_rotation_map(size_t orientation_index = 0)
{
    return HAL::GetHAL().screenRotation(orientation_index);
}

uint8_t screen_ui_orientation_index()
{
    return HAL::GetHAL().screenOrientationIndex();
}

bool screenComboIs(uint16_t combo_id)
{
    return screenCombo() == combo_id;
}

bool isXiaoDiyKit()
{
    return strcmp(HAL::GetHAL().boardProfile().device_family, "xiao_diy_kit") == 0;
}

bool usesDownOrientation()
{
    switch (screenCombo())
    {
    case 505:
    case 508:
    case 513:
    case 504:
    case 512:
    case 506:
    case 515:
    case 516:
        return true;
    default:
        return false;
    }
}

bool usesLargeLogo()
{
    switch (screenCombo())
    {
    case 522:
    case 523:
    case 510:
    case 511:
        return true;
    default:
        return false;
    }
}

bool usesInvertedQr()
{
    return screenComboIs(522) || screenComboIs(511);
}

bool usesFullActivationPage()
{
    return HAL::GetHAL().screenWidth() >= 800;
}

void begin_refresh()
{
    xSemaphoreTake(g_powerMutex, portMAX_DELAY);
    ++g_refreshSerial;
    g_refreshing = true;
    xSemaphoreGive(g_powerMutex);
}

uint32_t refresh_serial()
{
    xSemaphoreTake(g_powerMutex, portMAX_DELAY);
    uint32_t serial = g_refreshSerial;
    xSemaphoreGive(g_powerMutex);

    return serial;
}

void finish_refresh(uint32_t observed_serial)
{
    xSemaphoreTake(g_powerMutex, portMAX_DELAY);
    if (g_refreshing && g_refreshSerial == observed_serial)
    {
        g_refreshing = false;
    }
    xSemaphoreGive(g_powerMutex);
}

void hold_display_power()
{
    bool should_acquire = false;

    xSemaphoreTake(g_powerMutex, portMAX_DELAY);
    ++g_powerSerial;
    if (!g_powerHeld)
    {
        g_powerHeld = true;
        should_acquire = true;
    }
    xSemaphoreGive(g_powerMutex);

    if (should_acquire)
    {
        app_power_manager_acquire(AppPowerOwner::Display);
    }
}

uint32_t power_serial()
{
    xSemaphoreTake(g_powerMutex, portMAX_DELAY);
    uint32_t serial = g_powerSerial;
    xSemaphoreGive(g_powerMutex);

    return serial;
}

void release_display_power(uint32_t observed_serial)
{
    bool should_release = false;

    xSemaphoreTake(g_powerMutex, portMAX_DELAY);
    if (g_powerHeld && g_powerSerial == observed_serial)
    {
        g_powerHeld = false;
        should_release = true;
    }
    xSemaphoreGive(g_powerMutex);

    if (should_release)
    {
        app_power_manager_release(AppPowerOwner::Display);
    }
}

enum class HomeTextFillMode
{
    kTransparent,
    kOpaque
};

struct HomeTextMetrics
{
    int16_t width = 0;
    int16_t ascent = 0;
    int16_t descent = 0;

    int16_t height() const { return ascent + descent; }
};

static bool EnsureRenderFontLoaded(const uint8_t *fontData, size_t fontDataSize)
{
    static const uint8_t *loadedFontPtr = nullptr;
    static size_t loadedFontSize = 0;
    static bool drawerInitialized = false;

    if (!fontData || fontDataSize == 0)
    {
        return false;
    }

    if (!drawerInitialized)
    {
        render.setDrawer(display());
        drawerInitialized = true;
    }

    if (loadedFontPtr == fontData && loadedFontSize == fontDataSize)
    {
        return true;
    }

    if (loadedFontPtr != nullptr)
    {
        render.unloadFont();
        loadedFontPtr = nullptr;
        loadedFontSize = 0;
    }

    FT_Error error = render.loadFont(fontData, fontDataSize);
    if (error != FT_Err_Ok)
    {
        Log.errorln("[app_view] Failed to load OpenFontRender font (err=%d)", error);
        return false;
    }

    loadedFontPtr = fontData;
    loadedFontSize = fontDataSize;
    return true;
}

class GenericTextPainter
{
public:
    explicit GenericTextPainter(uint16_t backgroundColor)
        : backgroundColor_(backgroundColor)
    {
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setBackgroundColor(backgroundColor_);
    }

    template <typename Style>
    HomeTextMetrics MeasureText(const Style &style, const char *text)
    {
        ActiveEngine engine = PrepareStyle(style);

        HomeTextMetrics metrics{};
        if (engine == ActiveEngine::kU8G2)
        {
            metrics.width = u8g2Fonts.getUTF8Width(text);
            metrics.ascent = u8g2Fonts.getFontAscent();
            int16_t descent = u8g2Fonts.getFontDescent();
            metrics.descent = std::abs(descent);
        }
        else
        {
            uint16_t fontSize = style.renderFontSize > 0 ? style.renderFontSize : currentRenderFontSize_;
            if (fontSize == 0)
            {
                fontSize = 32;
            }
            FT_BBox topBox = render.calculateBoundingBox(0, 0, fontSize, Align::TopLeft, Layout::Horizontal, text);
            FT_BBox bottomBox = render.calculateBoundingBox(0, 0, fontSize, Align::BottomLeft, Layout::Horizontal, text);
            metrics.width = static_cast<int16_t>(topBox.xMax - topBox.xMin);

            int32_t height = static_cast<int32_t>(topBox.yMax - topBox.yMin);
            int32_t descentRaw = static_cast<int32_t>(bottomBox.yMax) / 2;
            int32_t descent = std::abs(descentRaw);
            int32_t ascent = height - descent;
            if (ascent < 0)
            {
                ascent = 0;
            }
            metrics.ascent = static_cast<int16_t>(ascent);
            metrics.descent = static_cast<int16_t>(descent);
        }
        return metrics;
    }

    template <typename Style>
    void DrawText(const Style &style,
                  const char *text,
                  int16_t x,
                  int16_t baselineY,
                  uint16_t fgColor,
                  HomeTextFillMode fillMode,
                  uint16_t bgColor,
                  const HomeTextMetrics *metrics = nullptr)
    {
        ActiveEngine engine = PrepareStyle(style);
        HomeTextMetrics localMetrics = metrics ? *metrics : MeasureText(style, text);

        if (engine == ActiveEngine::kU8G2)
        {
            if (fillMode == HomeTextFillMode::kTransparent)
            {
                u8g2Fonts.setFontMode(1);
                u8g2Fonts.setBackgroundColor(backgroundColor_);
            }
            else
            {
                u8g2Fonts.setFontMode(0);
                u8g2Fonts.setBackgroundColor(bgColor);
            }
            u8g2Fonts.setForegroundColor(fgColor);
            u8g2Fonts.setCursor(x, baselineY);
            u8g2Fonts.print(text);
        }
        else
        {
            uint16_t background = (fillMode == HomeTextFillMode::kTransparent) ? backgroundColor_ : bgColor;
            render.setFontColor(fgColor, TFT_WHITE);
            render.setBackgroundFillMethod(fillMode == HomeTextFillMode::kTransparent ? BgFillMethod::None : BgFillMethod::Block);
            int16_t topY = baselineY - localMetrics.ascent;
            render.drawString(text, x, topY, fgColor, background, Layout::Horizontal);
        }
    }

private:
    enum class ActiveEngine
    {
        kU8G2,
        kRender
    };

    template <typename Style>
    ActiveEngine PrepareStyle(const Style &style)
    {
        using EngineType = decltype(style.engine);

        bool canUseRender = (style.engine == EngineType::kOpenFontRender) &&
                            style.renderFontData != nullptr &&
                            style.renderFontDataSize > 0 &&
                            style.renderFontSize > 0 &&
                            EnsureRenderFontLoaded(style.renderFontData, style.renderFontDataSize);

        if (canUseRender)
        {
            ConfigureRenderDefaults();
            uint16_t fontSize = style.renderFontSize > 0 ? style.renderFontSize : 32;
            if (fontSize != currentRenderFontSize_)
            {
                render.setFontSize(fontSize);
                currentRenderFontSize_ = fontSize;
            }
            return ActiveEngine::kRender;
        }

        const uint8_t *font = style.u8g2Font ? style.u8g2Font : u8g2_font_helvR14_tf;
        if (font != currentU8g2Font_)
        {
            u8g2Fonts.setFont(font);
            currentU8g2Font_ = font;
        }
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setBackgroundColor(backgroundColor_);
        return ActiveEngine::kU8G2;
    }

    void ConfigureRenderDefaults()
    {
        if (!renderInitialized_)
        {
            render.setAlignment(Align::TopLeft);
            render.setLayout(Layout::Horizontal);
            renderInitialized_ = true;
        }
    }

    uint16_t backgroundColor_;
    const uint8_t *currentU8g2Font_ = nullptr;
    uint16_t currentRenderFontSize_ = 0;
    bool renderInitialized_ = false;
};

class HomePageTextPainter
{
public:
    enum class FontRole
    {
        kHeader,
        kMain,
        kInstructionNumber,
        kInstructionText,
        kQrLabel
    };

    HomePageTextPainter(const HomePageLayout &layout, uint16_t backgroundColor)
        : layout_(layout), painter_(backgroundColor) {}

    HomeTextMetrics MeasureText(FontRole role, const char *text)
    {
        return painter_.MeasureText(ResolveStyle(role), text);
    }

    void DrawText(FontRole role,
                  const char *text,
                  int16_t x,
                  int16_t baselineY,
                  uint16_t fgColor,
                  HomeTextFillMode fillMode,
                  uint16_t bgColor,
                  const HomeTextMetrics *metrics = nullptr)
    {
        painter_.DrawText(ResolveStyle(role), text, x, baselineY, fgColor, fillMode, bgColor, metrics);
    }

private:
    const HomePageTextStyle &ResolveStyle(FontRole role) const
    {
        switch (role)
        {
        case FontRole::kHeader:
            return layout_.headerStyle;
        case FontRole::kMain:
            return layout_.mainTextStyle;
        case FontRole::kInstructionNumber:
            return layout_.instructionNumberStyle;
        case FontRole::kInstructionText:
            return layout_.instructionTextStyle;
        case FontRole::kQrLabel:
            return layout_.qrLabelStyle;
        default:
            return layout_.mainTextStyle;
        }
    }

    const HomePageLayout &layout_;
    GenericTextPainter painter_;
};


} // namespace




static void view_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == APP_INPUT_EVENT_BASE)
    {
        switch (event_id)
        {
        case APP_INPUT_EVENT_NEXT_IMAGE:
        case APP_INPUT_EVENT_PREV_IMAGE:
        {
            app_view_request_gallery_refresh(static_cast<app_input_event_id_t>(event_id));
            break;
        }

        case APP_INPUT_EVENT_CLEAR_SCREEN:
            Log.infoln("[app_view] APP_INPUT_EVENT_CLEAR_SCREEN");
            esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_CLEAR, NULL, 1, portMAX_DELAY);
            break;

        default:
            break;
        }
        return;
    }

    if (event_base == VIEW_EVENT_BASE)
    {
        switch (event_id)
        {
        case VIEW_EVENT_SHOW_ACTIVATION_CODE:
            Log.infoln("[app_view] VIEW_EVENT_SHOW_ACTIVATION_CODE");
            if (event_data != nullptr)
            {
                memcpy(&g_savedActivationData, event_data, sizeof(ActivationEventData));
                begin_refresh();
                hold_display_power();
                xEventGroupSetBits(viewEventGroup, VIEW_EVT_SHOW_ACTIVATION);
            }
            break;

        case VIEW_EVENT_SHOW_IMAGE:
        {
            begin_refresh();
            hold_display_power();
            xEventGroupSetBits(viewEventGroup, VIEW_EVT_SHOW_IMAGE);
            break;
        }

        case VIEW_EVENT_SHOW_IMAGE_FAST:
        {
            begin_refresh();
            hold_display_power();
            xEventGroupSetBits(viewEventGroup, VIEW_EVT_SHOW_FAST);
            break;
        }

        case VIEW_EVENT_SHOW_STARTUP:
        {
            Log.infoln("[app_view] VIEW_EVENT_SHOW_STARTUP");
            uint8_t mac[6];
            WiFi.macAddress(mac);
            char mac_suffix[5];
            snprintf(mac_suffix, sizeof(mac_suffix), "%02x%02x", mac[4], mac[5]);

            const char *ap_prefix = HAL::GetHAL().apPrefix();
            String ap_ssid = String(ap_prefix ? ap_prefix : "ePaper DIY Kit") + "-" + mac_suffix;

            strncpy(g_savedApName, ap_ssid.c_str(), sizeof(g_savedApName) - 1);
            g_savedApName[sizeof(g_savedApName) - 1] = '\0';

            begin_refresh();
            hold_display_power();
            xEventGroupSetBits(viewEventGroup, VIEW_EVT_SHOW_STARTUP);

            break;
        }

        case VIEW_EVENT_SHOW_WAITING:
        {
            begin_refresh();
            hold_display_power();
            xEventGroupSetBits(viewEventGroup, VIEW_EVT_SHOW_WAIT);
            break;
        }

        case VIEW_EVENT_SHOW_DOWNLOADING:
        {
            xEventGroupSetBits(viewEventGroup, VIEW_EVT_SHOW_DOWNLOAD);
            break;
        }

        case VIEW_EVENT_SHOW_CLEAR:
        {
            Log.infoln("[app_view] VIEW_EVENT_SHOW_CLEAR");
            hold_display_power();
            xEventGroupSetBits(viewEventGroup, VIEW_EVT_SHOW_CLEAR);
            break;
        }
        case VIEW_EVENT_SHOW_ERROR:
        {
            Log.errorln("[app_view] VIEW_EVENT_SHOW_ERROR");
            const char *msg = static_cast<const char *>(event_data);
            if (msg != nullptr)
            {
                strncpy(g_errorMessage, msg, sizeof(g_errorMessage) - 1);
                g_errorMessage[sizeof(g_errorMessage) - 1] = '\0';
            }
            else
            {
                strncpy(g_errorMessage, "Failed to open file", sizeof(g_errorMessage) - 1);
                g_errorMessage[sizeof(g_errorMessage) - 1] = '\0';
            }
            begin_refresh();
            hold_display_power();
            xEventGroupSetBits(viewEventGroup, VIEW_EVT_SHOW_ERROR);
            break;
        }

        case VIEW_EVENT_SHOW_LOW_BATTERY:
        {
            Log.warningln("[app_view] VIEW_EVENT_SHOW_LOW_BATTERY");
            begin_refresh();
            hold_display_power();
            xEventGroupSetBits(viewEventGroup, VIEW_EVT_SHOW_LOW_BATTERY);
            break;
        }

        default:
            break;
        }
    }
}

static void drawIcon(int16_t x, int16_t y, const uint8_t *bmp, uint16_t iconW = 28, uint16_t iconH = 28)
{
    display().drawBitmap(x, y, bmp, iconW, iconH, TFT_BLACK);
}

static void finish_image_draw(bool is_dynamic)
{
    display().setRotation(screen_rotation_map(0));

    if (usesDownOrientation())
    {
        Log.verboseln("rotation id:%d", screen_rotation_map(3));
        display().setRotation(screen_rotation_map(3));
    }

    int16_t display_w = display().width();
    int16_t top_margin = 5;
    float pct = HAL::GetHAL().batteryReadPercent();
    int pct_int = static_cast<int>(pct + 0.5f);

    if (is_dynamic && WiFi.status() != WL_CONNECTED)
    {
        display().fillRect(display_w - top_margin - 70, top_margin, 28, 28, TFT_WHITE);
        drawIcon(display_w - top_margin - 70, top_margin, epd_bitmap_wifi);
    }
    if (pct_int < 3)
    {
        display().fillRect(display_w - top_margin - 30, top_margin, 28, 28, TFT_WHITE);
        drawIcon(display_w - top_margin - 30, top_margin, epd_bitmap_battery);
    }

    uint32_t display_start = millis();
    display().update();
    Log.infoln("[app_view] Display update completed in %lu ms", static_cast<unsigned long>(millis() - display_start));

}

static bool app_view_request_gallery_refresh(app_input_event_id_t event_id)
{
    app_power_manager_activity(AppPowerOwner::UserAction);

    const uint32_t now = millis();
    if ((now - g_last_gallery_click_ms) < kGalleryClickGuardMs)
    {
        Log.warningln("[app_view] Ignoring gallery click during guard window.");
        return false;
    }
    g_last_gallery_click_ms = now;

    const bool view_refreshing = app_view_is_refreshing();
    const bool wifi_provisioning = app_wifi_is_provisioning();
    const bool user_action_active = app_user_action_is_active();
    if (view_refreshing || wifi_provisioning || user_action_active)
    {
        Log.warningln("[app_view] Ignoring gallery click while device is busy. refreshing=%d provisioning=%d user_action=%d",
                      view_refreshing,
                      wifi_provisioning,
                      user_action_active);
        return false;
    }

    const bool selected = event_id == APP_INPUT_EVENT_NEXT_IMAGE
        ? app_gallery_select_next()
        : app_gallery_select_prev();
    if (!selected)
    {
        return false;
    }
    esp_err_t err = esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, NULL, 0, portMAX_DELAY);
    if (err != ESP_OK)
    {
        Log.errorln("[app_view] Failed to post image refresh event: err=%d", err);
        return false;
    }

    app_user_action_begin_refresh(true);
    return true;
}

static bool drawImageFromFileStream(int16_t x, int16_t y, bool with_color, bool overwrite, const char *filename, bool is_sd, bool is_dynamic)
{
    if (filename == nullptr || filename[0] == '\0')
    {
        Log.errorln("[app_view] Invalid filename for streaming decode");
        return false;
    }

    if (x >= display().width() || y >= display().height())
    {
        Log.errorln("[app_view] Invalid target coordinates for streaming decode");
        return false;
    }

    String fullPath = String("/") + String(filename);
    HAL::SharedSpiLock spiLock;
    File file = is_sd ? HAL::GetHAL().sdOpen(fullPath.c_str(), FILE_READ) : LittleFS.open(fullPath, FILE_READ);
    if (!file)
    {
        Log.errorln("[app_view] Failed to open %s for streaming decode", filename);
        esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_ERROR, NULL, 1, portMAX_DELAY);
        return false;
    }

    // At critically low battery, give the E1003 panel one white full refresh
    // before the new image. This clears residual charge without adding an
    // extra refresh during normal use.
    const float battery_percent = HAL::GetHAL().batteryReadPercent();
    if (screenComboIs(522) && battery_percent >= 0.0f && battery_percent < 5.0f)
    {
#if defined(USE_MUTIGRAY_EPAPER)
        display().deinitGrayMode();
#endif
        display().setRotation(screen_rotation_map(0));
        display().fillSprite(TFT_WHITE);
        uint32_t clean_start = millis();
        display().update();
        Log.infoln("[app_view] E1003 low-battery ghost-clean refresh completed in %lu ms (battery=%.1f%%)",
                   static_cast<unsigned long>(millis() - clean_start),
                   battery_percent);
    }

    bool valid = image_parser::draw_from_stream(file, x, y, with_color, overwrite);

    file.close();

    if (valid)
    {
        finish_image_draw(is_dynamic);
    }
    request_update_timer_start();
    return valid;
}

bool imgBufferMutex_lock()
{
    return (xSemaphoreTake(imageBufferMutex, portMAX_DELAY) == pdTRUE);
}

void imgBufferMutex_unlock()
{
    xSemaphoreGive(imageBufferMutex);
}

static void displayHomeUI(const unsigned char *qrBitmap1, const unsigned char *qrBitmap2, int qrWidth, int qrHeight, const char *hotspotName)
{
    HAL::SharedSpiLock spiLock;
    const bool isColorScreen = HAL::GetHAL().screenIsColor();
    const uint16_t BG_COLOR = isColorScreen ? TFT_GREEN : TFT_WHITE;
    const uint16_t MAIN_TEXT_COLOR = isColorScreen ? TFT_WHITE : TFT_BLACK;
    const uint16_t QR_CODE_COLOR = isColorScreen ? TFT_WHITE : TFT_BLACK;
    const uint16_t NUMBER_IN_BOX_COLOR = isColorScreen ? TFT_GREEN : TFT_WHITE;
    const uint16_t BOX_COLOR = isColorScreen ? TFT_WHITE : TFT_BLACK;
    const uint16_t HEADER_TEXT_COLOR = isColorScreen ? TFT_WHITE : TFT_BLACK;
    const uint16_t OTHER_ELEMENT_COLOR = isColorScreen ? TFT_WHITE : TFT_BLACK;

    const auto &layout = kHomePageLayout;

    const int16_t w = display().width();

    display().fillRect(0, 0, display().width(), display().height(), BG_COLOR);

    HomePageTextPainter textPainter(layout, BG_COLOR);

    const char *headerText = HAL::GetHAL().boardProfile().display_name;

    if (headerText != nullptr)
    {
        auto headerMetrics = textPainter.MeasureText(HomePageTextPainter::FontRole::kHeader, headerText);
        textPainter.DrawText(HomePageTextPainter::FontRole::kHeader,
                             headerText,
                             layout.headerText.x,
                             layout.headerText.baseline,
                             HEADER_TEXT_COLOR,
                             HomeTextFillMode::kTransparent,
                             BG_COLOR,
                             &headerMetrics);
    }

    // Logo and segment line
    if (usesLargeLogo())
    {
        display().drawBitmap(layout.logoX, layout.logoY, seeed_logo_600_130, layout.logoWidth, layout.logoHeight, BG_COLOR, MAIN_TEXT_COLOR);
    }
    else
    {
        display().drawBitmap(layout.logoX, layout.logoY, epd_bitmap_seeed_logo, layout.logoWidth, layout.logoHeight, OTHER_ELEMENT_COLOR);
    }
    int16_t headerLineX = layout.headerLineUseDisplayWidth ? 0 : layout.headerLineStartX;
    int16_t headerLineWidth = layout.headerLineUseDisplayWidth ? w : layout.headerLineWidth;
    if (headerLineWidth <= 0)
    {
        headerLineWidth = w;
    }
    display().drawFastHLine(headerLineX, layout.headerLineY, headerLineWidth, OTHER_ELEMENT_COLOR);

    // main text
    const char *line1_text = "Powered by SenseCraft HMI";
    const char *line2_text = "Create Your Personal Display";
    auto line1Metrics = textPainter.MeasureText(HomePageTextPainter::FontRole::kMain, line1_text);
    auto line2Metrics = textPainter.MeasureText(HomePageTextPainter::FontRole::kMain, line2_text);

    if (layout.mainLine1.baseline > 0)
    {
        textPainter.DrawText(HomePageTextPainter::FontRole::kMain,
                             line1_text,
                             layout.mainLine1.x,
                             layout.mainLine1.baseline,
                             MAIN_TEXT_COLOR,
                             HomeTextFillMode::kTransparent,
                             BG_COLOR,
                             &line1Metrics);
    }
    if (layout.mainLine2.baseline > 0)
    {
        textPainter.DrawText(HomePageTextPainter::FontRole::kMain,
                             line2_text,
                             layout.mainLine2.x,
                             layout.mainLine2.baseline,
                             MAIN_TEXT_COLOR,
                             HomeTextFillMode::kTransparent,
                             BG_COLOR,
                             &line2Metrics);
    }

    // instruction text
    const int16_t boxSize = layout.instructionBoxSize;
    const int16_t cornerRadius = layout.instructionCornerRadius;

    auto drawInstructionStep = [&](int step_number, const HomePageInstructionStepPosition &position, const char *step_text) {
        if (boxSize <= 0)
        {
            return;
        }
        if (position.boxX < 0 || position.boxY < 0)
        {
            return;
        }

        display().fillRoundRect(position.boxX, position.boxY, boxSize, boxSize, cornerRadius, BOX_COLOR);

        char numberBuffer[4];
        snprintf(numberBuffer, sizeof(numberBuffer), "%d", step_number);

        auto numberMetrics = textPainter.MeasureText(HomePageTextPainter::FontRole::kInstructionNumber, numberBuffer);
        int16_t numberX = position.boxX + (boxSize - numberMetrics.width) / 2;
        int16_t numberBaseline = position.boxY + (boxSize + numberMetrics.height()) / 2 - numberMetrics.descent;

        textPainter.DrawText(HomePageTextPainter::FontRole::kInstructionNumber,
                             numberBuffer,
                             numberX,
                             numberBaseline,
                             NUMBER_IN_BOX_COLOR,
                             HomeTextFillMode::kOpaque,
                             BOX_COLOR,
                             &numberMetrics);

        textPainter.DrawText(HomePageTextPainter::FontRole::kInstructionText,
                             step_text,
                             position.textX,
                             position.textBaseline,
                             MAIN_TEXT_COLOR,
                             HomeTextFillMode::kTransparent,
                             BG_COLOR);
    };

    char textLine5[64];
    snprintf(textLine5, sizeof(textLine5), "Connect to the Wi-Fi: '%s'", hotspotName);
    if (!layout.instructionSteps.empty())
    {
        drawInstructionStep(1, layout.instructionSteps[0], textLine5);
    }

    const char manualPortalText[] = "Open \"192.168.4.1\" on your browser or scan the QR code";
    if (layout.instructionSteps.size() > 1)
    {
        drawInstructionStep(2, layout.instructionSteps[1], manualPortalText);
    }

    const char stepThreeText[] = "Select your local WiFi network and enter the password.";
    if (layout.instructionSteps.size() > 2)
    {
        drawInstructionStep(3, layout.instructionSteps[2], stepThreeText);
    }

    // QR code block and captions
    const char *qrLabels[2] = {"User Guide", "Wi-Fi Setup"};
    const unsigned char *qrBitmaps[2] = {qrBitmap1, qrBitmap2};
    HomeTextMetrics qrLabelMetrics[2] = {
        textPainter.MeasureText(HomePageTextPainter::FontRole::kQrLabel, qrLabels[0]),
        textPainter.MeasureText(HomePageTextPainter::FontRole::kQrLabel, qrLabels[1])};

    auto drawQrBlock = [&](int blockIndex) {
        if (blockIndex >= static_cast<int>(layout.qrBlocks.size()))
        {
            return;
        }
        const auto &qrPos = layout.qrBlocks[blockIndex];
        if (qrPos.x < 0 || qrPos.y < 0)
        {
            return;
        }
        int16_t labelBaseline = (qrPos.labelBaseline >= 0) ? qrPos.labelBaseline : (qrPos.y + qrHeight + 32);
        int16_t labelX = qrPos.labelX;
        const auto &metrics = qrLabelMetrics[blockIndex];
        if (labelX < 0)
        {
            labelX = qrPos.x + (qrWidth - metrics.width) / 2;
        }

        if (usesInvertedQr())
        {
            display().drawBitmap(qrPos.x, qrPos.y, qrBitmaps[blockIndex], qrWidth, qrHeight, TFT_WHITE, TFT_BLACK);
        }
        else
        {
            display().drawBitmap(qrPos.x, qrPos.y, qrBitmaps[blockIndex], qrWidth, qrHeight, QR_CODE_COLOR);
        }
        textPainter.DrawText(HomePageTextPainter::FontRole::kQrLabel,
                             qrLabels[blockIndex],
                             labelX,
                             labelBaseline,
                             MAIN_TEXT_COLOR,
                             HomeTextFillMode::kTransparent,
                             BG_COLOR,
                             &metrics);
    };

    drawQrBlock(0);
    drawQrBlock(1);

    uint32_t updateStart = millis();
    Log.infoln("[app_view] Startup UI display update begin: combo=%u, size=%dx%d",
               screenCombo(),
               display().width(),
               display().height());
    display().update();
    Log.infoln("[app_view] Startup UI display update returned in %u ms",
               static_cast<unsigned long>(millis() - updateStart));
}


static int16_t ComputeAlignedX(int16_t textWidth, DIYTextAlignment alignment, int16_t margin, int16_t screenWidth)
{
    switch (alignment)
    {
    case DIYTextAlignment::kLeft:
        return margin;
    case DIYTextAlignment::kRight:
        return screenWidth - margin - textWidth;
    case DIYTextAlignment::kCenter:
    default:
        return (screenWidth - textWidth) / 2;
    }
}


static void DrawRoundedRectBorder(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, int16_t thickness, uint16_t color)
{
    if (thickness <= 0)
    {
        return;
    }

    int16_t layers = thickness;
    int16_t max_layers = w < h ? w / 2 : h / 2;
    if (layers > max_layers)
    {
        layers = max_layers;
    }

    for (int16_t i = 0; i < layers; ++i)
    {
        int16_t current_radius = radius - i;
        if (current_radius < 0)
        {
            current_radius = 0;
        }
        display().drawRoundRect(x + i, y + i, w - 2 * i, h - 2 * i, current_radius, color);
    }
}

static void diy_displayHomeUI(uint32_t index, const char *hotspotName, const char *bodyPrefix)
{
    HAL::SharedSpiLock spiLock;
    const auto &layout = kDIYHomePageLayout;
    const int16_t screen_w = display().width();
    const int16_t screen_h = display().height();
    const char *safeHotspotName = hotspotName ? hotspotName : "";

    // display().fillSprite(layout.backgroundColor);
    display().fillRect(0, 0, screen_w, screen_h, layout.backgroundColor);
    u8g2Fonts.setFontMode(0);
    u8g2Fonts.setBackgroundColor(layout.backgroundColor);

    // Header / title line.
    int16_t title_baseline = layout.titleTopPadding;
    bool title_drawn = false;
    if (layout.titleFont != nullptr && layout.titleText != nullptr)
    {
        u8g2Fonts.setFont(layout.titleFont);
        u8g2Fonts.setForegroundColor(layout.titleTextColor);
        int16_t title_w = u8g2Fonts.getUTF8Width(layout.titleText);
        int16_t title_ascent = u8g2Fonts.getFontAscent();
        int16_t title_x = ComputeAlignedX(title_w, layout.titleAlignment, layout.titleLeftMargin, screen_w);
        title_baseline = layout.titleTopPadding + title_ascent;
        u8g2Fonts.setCursor(title_x, title_baseline);
        u8g2Fonts.print(layout.titleText);
        title_drawn = true;
    }

    // Optional sub-title line.
    if (layout.subtitleFont != nullptr && layout.subtitleText != nullptr)
    {
        u8g2Fonts.setFont(layout.subtitleFont);
        u8g2Fonts.setForegroundColor(layout.subtitleTextColor);
        int16_t subtitle_w = u8g2Fonts.getUTF8Width(layout.subtitleText);
        int16_t subtitle_ascent = u8g2Fonts.getFontAscent();
        int16_t subtitle_x = ComputeAlignedX(subtitle_w, layout.subtitleAlignment, layout.subtitleLeftMargin, screen_w);
        int16_t reference_baseline = title_drawn ? title_baseline : layout.titleTopPadding;
        int16_t subtitle_baseline = reference_baseline + layout.subtitleGapBelowTitle + subtitle_ascent;
        u8g2Fonts.setCursor(subtitle_x, subtitle_baseline);
        u8g2Fonts.print(layout.subtitleText);
    }

    // Index badge.
    int16_t box_x = ComputeAlignedX(layout.indexBoxWidth, layout.indexAlignment, layout.indexLeftMargin, screen_w);
    int16_t box_y = layout.indexTopPadding;
    display().fillRoundRect(box_x, box_y, layout.indexBoxWidth, layout.indexBoxHeight, layout.indexBoxCornerRadius, layout.indexBoxFillColor);
    DrawRoundedRectBorder(box_x, box_y, layout.indexBoxWidth, layout.indexBoxHeight, layout.indexBoxCornerRadius, layout.indexBoxBorderThickness, layout.indexBoxBorderColor);

    u8g2Fonts.setFont(layout.indexFont);
    u8g2Fonts.setFontMode(0);
    u8g2Fonts.setBackgroundColor(layout.indexBoxFillColor);
    u8g2Fonts.setForegroundColor(layout.indexTextColor);
    char index_buffer[12];
    snprintf(index_buffer, sizeof(index_buffer), "%u", static_cast<unsigned int>(index));
    int16_t index_w = u8g2Fonts.getUTF8Width(index_buffer);
    int16_t index_ascent = u8g2Fonts.getFontAscent();
    int16_t index_baseline = box_y + (layout.indexBoxHeight + index_ascent) / 2 + layout.indexBaselineAdjust;
    int16_t index_x = box_x + (layout.indexBoxWidth - index_w) / 2;
    u8g2Fonts.setCursor(index_x, index_baseline);
    u8g2Fonts.print(index_buffer);
    u8g2Fonts.setBackgroundColor(layout.backgroundColor);

    // Footer / body text.
    if (layout.bodyFont != nullptr)
    {
        u8g2Fonts.setFont(layout.bodyFont);
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setBackgroundColor(layout.backgroundColor);
        u8g2Fonts.setForegroundColor(layout.bodyTextColor);
        const char *effectivePrefix = (bodyPrefix != nullptr) ? bodyPrefix : layout.bodyPrefix;
        const bool has_effective_prefix = (effectivePrefix != nullptr) && effectivePrefix[0] != '\0';
        const bool has_hotspot = safeHotspotName[0] != '\0';
        const bool has_prefix_or_hotspot = has_effective_prefix || has_hotspot;
        String line;
        if (has_effective_prefix)
        {
            line += effectivePrefix;
            if (has_hotspot && effectivePrefix[strlen(effectivePrefix) - 1] != ' ')
            {
                line += ' ';
            }
        }

        if (has_hotspot)
        {
            if (layout.bodyWrapHotspotInQuotes)
            {
                line += "\"";
                line += safeHotspotName;
                line += "\"";
            }
            else
            {
                line += safeHotspotName;
            }
        }

        if (layout.bodySuffix && layout.bodySuffix[0] != '\0')
        {
            line += layout.bodySuffix;
        }

        if (!has_prefix_or_hotspot && !(layout.bodySuffix && layout.bodySuffix[0] != '\0'))
        {
            display().update();
            return;
        }

        if (line.length() == 0)
        {
            // Only suffix remains; keep spacing consistent.
            display().update();
            return;
        }

        int16_t body_ascent = u8g2Fonts.getFontAscent();
        int16_t body_baseline = 0;

        if (layout.bodyPlacement == DIYBodyPlacement::kBelowIndex)
        {
            if (layout.bodyBottomOffset >= 0)
            {
                body_baseline = screen_h - layout.bodyBottomOffset;
            }
            else
            {
                body_baseline = box_y + layout.indexBoxHeight + layout.bodyGapFromIndex + body_ascent;
            }
            int16_t available_width = screen_w - (layout.bodyLeftMargin * 2);
            if (available_width <= 0)
            {
                available_width = screen_w;
            }

            std::vector<String> wrapped_lines;
            std::vector<int16_t> wrapped_widths;
            String remaining = line;
            while (!remaining.isEmpty())
            {
                int best_break = 0;
                int16_t last_width = 0;
                for (int i = 1; i <= remaining.length(); ++i)
                {
                    String candidate = remaining.substring(0, i);
                    int16_t candidate_width = u8g2Fonts.getUTF8Width(candidate.c_str());
                    if (candidate_width > available_width)
                    {
                        break;
                    }
                    last_width = candidate_width;
                    best_break = i;
                }

                bool wrapped_due_to_width = false;
                if (best_break == 0)
                {
                    best_break = remaining.length();
                    last_width = u8g2Fonts.getUTF8Width(remaining.c_str());
                }
                else
                {
                    wrapped_due_to_width = best_break < remaining.length();
                }

                if (wrapped_due_to_width)
                {
                    int space_pos = remaining.lastIndexOf(' ', best_break - 1);
                    if (space_pos >= 0)
                    {
                        best_break = space_pos + 1;
                        String candidate = remaining.substring(0, best_break);
                        last_width = u8g2Fonts.getUTF8Width(candidate.c_str());
                    }
                }

                String line_to_draw = remaining.substring(0, best_break);
                remaining = remaining.substring(best_break);
                while (remaining.length() > 0 && remaining.charAt(0) == ' ')
                {
                    remaining.remove(0, 1);
                }

                wrapped_lines.push_back(line_to_draw);
                wrapped_widths.push_back(last_width);
            }

            if (!wrapped_lines.empty())
            {
                int16_t line_height = u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent();
                if (line_height <= 0)
                {
                    line_height = u8g2Fonts.getFontAscent();
                }

                if (layout.bodyBottomOffset >= 0)
                {
                    body_baseline = screen_h - layout.bodyBottomOffset - (static_cast<int>(wrapped_lines.size()) - 1) * line_height;
                }

                for (size_t idx = 0; idx < wrapped_lines.size(); ++idx)
                {
                    int16_t line_width = wrapped_widths[idx];
                    int16_t line_x = ComputeAlignedX(line_width, layout.bodyAlignment, layout.bodyLeftMargin, screen_w);
                    u8g2Fonts.setCursor(line_x, body_baseline);
                    u8g2Fonts.print(wrapped_lines[idx]);
                    body_baseline += line_height;
                }
            }
        }
        else // DIYBodyPlacement::kRightOfIndex
        {
            if (layout.bodyBottomOffset >= 0)
            {
                body_baseline = screen_h - layout.bodyBottomOffset;
            }
            else
            {
                body_baseline = box_y + (layout.indexBoxHeight + body_ascent) / 2;
            }

            int16_t anchor_left = box_x + layout.indexBoxWidth + layout.bodyGapFromIndex;
            int16_t padded_left = anchor_left + layout.bodyLeftMargin;
            int16_t padded_right = screen_w - layout.bodyLeftMargin;
            int16_t available_width = padded_right - padded_left;
            if (available_width <= 0)
            {
                available_width = 0;
            }

            std::vector<String> wrapped_lines;
            std::vector<int16_t> wrapped_widths;
            String remaining = line;
            while (!remaining.isEmpty())
            {
                int best_break = 0;
                int16_t last_width = 0;
                for (int i = 1; i <= remaining.length(); ++i)
                {
                    String candidate = remaining.substring(0, i);
                    int16_t candidate_width = u8g2Fonts.getUTF8Width(candidate.c_str());
                    if (candidate_width > available_width)
                    {
                        break;
                    }
                    last_width = candidate_width;
                    best_break = i;
                }

                bool wrapped_due_to_width = false;
                if (best_break == 0)
                {
                    best_break = remaining.length();
                    last_width = u8g2Fonts.getUTF8Width(remaining.c_str());
                }
                else
                {
                    wrapped_due_to_width = best_break < remaining.length();
                }

                if (wrapped_due_to_width)
                {
                    int space_pos = remaining.lastIndexOf(' ', best_break - 1);
                    if (space_pos >= 0)
                    {
                        best_break = space_pos + 1;
                        String candidate = remaining.substring(0, best_break);
                        last_width = u8g2Fonts.getUTF8Width(candidate.c_str());
                    }
                }

                String line_to_draw = remaining.substring(0, best_break);
                remaining = remaining.substring(best_break);
                while (remaining.length() > 0 && remaining.charAt(0) == ' ')
                {
                    remaining.remove(0, 1);
                }

                wrapped_lines.push_back(line_to_draw);
                wrapped_widths.push_back(last_width);
            }

            if (!wrapped_lines.empty())
            {
                int16_t line_height = u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent();
                if (line_height <= 0)
                {
                    line_height = u8g2Fonts.getFontAscent();
                }

                if (layout.bodyBottomOffset >= 0)
                {
                    body_baseline = screen_h - layout.bodyBottomOffset - (static_cast<int>(wrapped_lines.size()) - 1) * line_height;
                }
                else
                {
                    int16_t block_height = line_height * static_cast<int>(wrapped_lines.size());
                    int16_t block_top = box_y + (layout.indexBoxHeight - block_height) / 2;
                    body_baseline = block_top + body_ascent;
                }

                for (size_t idx = 0; idx < wrapped_lines.size(); ++idx)
                {
                    int16_t line_width = wrapped_widths[idx];
                    int16_t line_x = padded_left;
                    switch (layout.bodyAlignment)
                    {
                    case DIYTextAlignment::kLeft:
                        line_x = anchor_left + layout.bodyLeftMargin;
                        break;
                    case DIYTextAlignment::kRight:
                        line_x = padded_right - line_width;
                        break;
                    case DIYTextAlignment::kCenter:
                    default:
                        line_x = padded_left + (available_width - line_width) / 2;
                        break;
                    }

                    u8g2Fonts.setCursor(line_x, body_baseline);
                    u8g2Fonts.print(wrapped_lines[idx]);
                    body_baseline += line_height;
                }
            }
        }
    }

    display().update();
}


static void diy_displayActivationCodePage(int activationCode, const String &hmiUrl)
{
    HAL::SharedSpiLock spiLock;
    if (isXiaoDiyKit())
    {
        display().setRotation(screen_rotation_map(3));
    }
    const auto &layout = kDIYActivationCodePageLayout;
    const uint16_t BG_COLOR = layout.backgroundColor;
    const uint16_t TEXT_COLOR = layout.textColor;
    const uint16_t CODE_COLOR = layout.codeTextColor;

    const int16_t screen_w = display().width();
    const int16_t screen_h = display().height();

    display().fillRect(0, 0, screen_w, screen_h, BG_COLOR);

    u8g2Fonts.setFontMode(0);
    u8g2Fonts.setBackgroundColor(BG_COLOR);
    u8g2Fonts.setForegroundColor(TEXT_COLOR);

    // Title: configurable font, alignment, and distance from the top edge.
    u8g2Fonts.setFont(layout.titleFont);
    const char *title_text = layout.titleText ? layout.titleText : "Pair Code";
    int16_t title_w = u8g2Fonts.getUTF8Width(title_text);
    int16_t title_ascent = u8g2Fonts.getFontAscent();
    int16_t title_x = ComputeAlignedX(title_w, layout.titleAlignment, layout.titleLeftMargin, screen_w);
    int16_t title_baseline = layout.titleTopPadding + title_ascent;
    u8g2Fonts.setCursor(title_x, title_baseline);
    u8g2Fonts.print(title_text);

    // Format the activation code as digits separated with dashes (e.g. 1-2-3-4).
    char code_raw[12];
    snprintf(code_raw, sizeof(code_raw), "%d", activationCode);
    size_t raw_len = strlen(code_raw);
    char code_display[24];
    size_t display_idx = 0;
    for (size_t i = 0; i < raw_len && display_idx + 1 < sizeof(code_display); ++i)
    {
        code_display[display_idx++] = code_raw[i];
        if (i < raw_len - 1 && display_idx + 1 < sizeof(code_display))
        {
            code_display[display_idx++] = '-';
        }
    }
    code_display[display_idx] = '\0';

    u8g2Fonts.setFont(layout.codeFont);
    u8g2Fonts.setForegroundColor(CODE_COLOR);
    int16_t code_w = u8g2Fonts.getUTF8Width(code_display);
    int16_t code_ascent = u8g2Fonts.getFontAscent();
    int16_t code_x = ComputeAlignedX(code_w, layout.codeAlignment, layout.codeLeftMargin, screen_w);
    int16_t code_baseline = title_baseline + layout.codeGapBelowTitle + code_ascent;
    u8g2Fonts.setCursor(code_x, code_baseline);
    u8g2Fonts.print(code_display);
    u8g2Fonts.setForegroundColor(TEXT_COLOR);

    // Instructional text.
    u8g2Fonts.setFont(layout.infoFont);
    const char *instruction_text = layout.infoText ? layout.infoText : "Add device on";
    int16_t instruction_w = u8g2Fonts.getUTF8Width(instruction_text);
    int16_t instruction_ascent = u8g2Fonts.getFontAscent();
    int16_t instruction_baseline = 0;
    if (layout.infoBottomOffset >= 0)
    {
        instruction_baseline = screen_h - layout.infoBottomOffset;
    }
    else
    {
        instruction_baseline = code_baseline + layout.infoGapBelowCode + instruction_ascent;
    }
    int16_t instruction_x = ComputeAlignedX(instruction_w, layout.infoAlignment, layout.infoLeftMargin, screen_w);
    u8g2Fonts.setCursor(instruction_x, instruction_baseline);
    u8g2Fonts.print(instruction_text);

    // Highlighted URL line.
    u8g2Fonts.setFont(layout.urlFont);
    int16_t url_w = u8g2Fonts.getUTF8Width(hmiUrl.c_str());
    int16_t url_ascent = u8g2Fonts.getFontAscent();
    int16_t url_baseline = 0;
    if (layout.urlBottomOffset >= 0)
    {
        url_baseline = screen_h - layout.urlBottomOffset;
    }
    else
    {
        url_baseline = instruction_baseline + layout.urlGapBelowInfo + url_ascent;
    }
    int16_t url_x = ComputeAlignedX(url_w, layout.urlAlignment, layout.urlLeftMargin, screen_w);
    u8g2Fonts.setCursor(url_x, url_baseline);
    u8g2Fonts.print(hmiUrl);
    display().drawFastHLine(url_x, url_baseline + layout.urlUnderlineOffset, url_w, TEXT_COLOR);

    display().update();
    display().setRotation(screen_rotation_map(0));
}

static void displayActivationCodePage(int activationCode, String hmiUrl)
{
    HAL::SharedSpiLock spiLock;
    const bool isColorScreen = HAL::GetHAL().screenIsColor();
    const uint16_t SCREEN_BG_COLOR = TFT_WHITE;
    const uint16_t DEFAULT_FG_COLOR = TFT_BLACK;
    const uint16_t BOX_FILL_COLOR = TFT_GREEN;
    const uint16_t DIGIT_COLOR = isColorScreen ? TFT_WHITE : TFT_BLACK;

    const auto &layout = kActivationCodePageLayout;
    const int16_t screenWidth = display().width();

    display().fillSprite(SCREEN_BG_COLOR);

    String codeStr = String(activationCode);
    int num_digits = codeStr.length();

    GenericTextPainter textPainter(SCREEN_BG_COLOR);

    auto computeAlignedX = [&](const ActivationCodeTextPosition &pos, int16_t textWidth) -> int16_t {
        if (pos.centered)
        {
            return (screenWidth - textWidth) / 2;
        }
        return pos.x;
    };

    auto fillTextBackground = [&](int16_t x, int16_t baseline, const HomeTextMetrics &metrics, uint16_t color) {
        if (metrics.width <= 0 || metrics.height() <= 0)
        {
            return;
        }
        int16_t top = baseline - metrics.ascent;
        if (top < 0)
        {
            top = 0;
        }
        display().fillRect(x, top, metrics.width, metrics.height(), color);
    };

    if (layout.titleText != nullptr)
    {
        auto titleMetrics = textPainter.MeasureText(layout.titleStyle, layout.titleText);
        int16_t titleX = computeAlignedX(layout.titlePosition, titleMetrics.width);
        fillTextBackground(titleX, layout.titlePosition.baseline, titleMetrics, SCREEN_BG_COLOR);
        textPainter.DrawText(layout.titleStyle,
                             layout.titleText,
                             titleX,
                             layout.titlePosition.baseline,
                             DEFAULT_FG_COLOR,
                             HomeTextFillMode::kOpaque,
                             SCREEN_BG_COLOR,
                             &titleMetrics);
    }

    const auto &digitsLayout = layout.digits;
    const int16_t boxWidth = digitsLayout.boxWidth > 0 ? digitsLayout.boxWidth : 100;
    const int16_t boxHeight = digitsLayout.boxHeight > 0 ? digitsLayout.boxHeight : boxWidth;
    const int16_t spacing = digitsLayout.spacing >= 0 ? digitsLayout.spacing : 0;
    const int16_t rowTop = digitsLayout.rowTop >= 0 ? digitsLayout.rowTop : (display().height() / 4);
    int16_t startX = digitsLayout.centerRow ? (screenWidth - (num_digits * boxWidth + (num_digits - 1) * spacing)) / 2 : digitsLayout.startX;
    if (startX < 0)
    {
        startX = 0;
    }
    const int16_t cornerRadius = std::max<int16_t>(0, digitsLayout.boxCornerRadius);
    const int16_t connectorGap = std::max<int16_t>(0, digitsLayout.connectorGap);
    const int16_t connectorThickness = std::max<int16_t>(0, digitsLayout.connectorThickness);
    const int16_t userConnectorLength = digitsLayout.connectorLength;

    for (int i = 0; i < num_digits; ++i)
    {
        int16_t boxX = startX + i * (boxWidth + spacing);
        uint16_t digitBg = SCREEN_BG_COLOR;
        if (isColorScreen)
        {
            display().fillRoundRect(boxX, rowTop, boxWidth, boxHeight, cornerRadius, BOX_FILL_COLOR);
            digitBg = BOX_FILL_COLOR;
        }
        else
        {
            display().drawRoundRect(boxX, rowTop, boxWidth, boxHeight, cornerRadius, DEFAULT_FG_COLOR);
        }
        char digit[2] = {codeStr[i], '\0'};
        auto digitMetrics = textPainter.MeasureText(digitsLayout.textStyle, digit);
        int16_t digitBaseline = rowTop + (boxHeight + digitMetrics.height()) / 2 - digitMetrics.descent + digitsLayout.digitBaselineAdjust;
        int16_t digitX = boxX + (boxWidth - digitMetrics.width) / 2;
        textPainter.DrawText(digitsLayout.textStyle,
                             digit,
                             digitX,
                             digitBaseline,
                             DIGIT_COLOR,
                             HomeTextFillMode::kOpaque,
                             digitBg,
                             &digitMetrics);

        if (i < num_digits - 1 && connectorThickness > 0)
        {
            int16_t connectorWidth = userConnectorLength > 0 ? userConnectorLength : (spacing - connectorGap * 2);
            if (connectorWidth > 0)
            {
                int16_t connectorLeft = boxX + boxWidth + connectorGap;
                int16_t connectorY = rowTop + (boxHeight - connectorThickness) / 2;
                display().fillRect(connectorLeft, connectorY, connectorWidth, connectorThickness, DEFAULT_FG_COLOR);
            }
        }
    }

    String urlText = "Register your device on " + hmiUrl;
    auto infoMetrics = textPainter.MeasureText(layout.infoStyle, urlText.c_str());
    int16_t infoX = computeAlignedX(layout.infoPosition, infoMetrics.width);
    fillTextBackground(infoX, layout.infoPosition.baseline, infoMetrics, SCREEN_BG_COLOR);
    textPainter.DrawText(layout.infoStyle,
                         urlText.c_str(),
                         infoX,
                         layout.infoPosition.baseline,
                         DEFAULT_FG_COLOR,
                         HomeTextFillMode::kOpaque,
                         SCREEN_BG_COLOR,
                         &infoMetrics);
    auto hmiMetrics = textPainter.MeasureText(layout.infoStyle, hmiUrl.c_str());
    if (hmiMetrics.width > 0)
    {
        int16_t underlineStart = infoX + infoMetrics.width - hmiMetrics.width;
        display().drawFastHLine(underlineStart,
                              layout.infoPosition.baseline + layout.underlineOffset,
                              hmiMetrics.width,
                              DEFAULT_FG_COLOR);
    }

    display().update();
}

static void init_fonts()
{
    u8g2Fonts.begin(static_cast<TFT_eSPI &>(display()));
    uint16_t bg = TFT_WHITE;
    uint16_t fg = TFT_BLACK;
    u8g2Fonts.setForegroundColor(fg);
    u8g2Fonts.setBackgroundColor(bg);

    render.setSerial(Serial);
    render.showFreeTypeVersion();
    render.showCredit();
}

namespace
{
    bool g_view_task_registered = false;
    static TaskHandle_t g_startup_sequence_task = nullptr;
    static String g_last_image_name;
    static String g_current_image_name;
    static bool g_showing_activation = false;

    void stop_startup_sequence_task(bool wait_for_stop)
    {
        TaskHandle_t task = g_startup_sequence_task;
        if (task == nullptr)
        {
            return;
        }

        xTaskNotifyGive(task);

        if (wait_for_stop)
        {
            constexpr uint32_t kTimeoutMs = 300;
            uint32_t waited_ms = 0;
            while (g_startup_sequence_task != nullptr && waited_ms < kTimeoutMs)
            {
                delay(10);
                waited_ms += 10;
            }

            if (g_startup_sequence_task != nullptr)
            {
                vTaskDelete(task);
                g_startup_sequence_task = nullptr;
            }
        }
    }

    void startup_sequence_task(void *)
    {

        constexpr TickType_t kPageDelay = pdMS_TO_TICKS(5000);
        uint32_t page = 0;
        while (1)
        {
            if (ulTaskNotifyTake(pdTRUE, 0) > 0)
            {
                break;
            }

            const uint32_t page_index = (page % 3) + 1;
            switch (page_index)
            {
            case 1:
            {
                AppPowerGuard display_guard(AppPowerOwner::Display);
                diy_displayHomeUI(page_index, g_savedApName, "Connect to the Wi-Fi:");
                break;
            }
            case 2:
            {
                AppPowerGuard display_guard(AppPowerOwner::Display);
                diy_displayHomeUI(page_index, "Open \"192.168.4.1\" on your browser", nullptr);
                break;
            }
            case 3:
            default:
            {
                AppPowerGuard display_guard(AppPowerOwner::Display);
                diy_displayHomeUI(page_index, "Select your local WiFi and enter the password", nullptr);
                break;
            }
            }

            ++page;

            if (ulTaskNotifyTake(pdTRUE, kPageDelay) > 0)
            {
                break;
            }
        }

        if (xTaskGetCurrentTaskHandle() == g_startup_sequence_task)
        {
            g_startup_sequence_task = nullptr;
        }

        vTaskDelete(nullptr);
    }

    void show_activation()
    {
        int code = g_savedActivationData.activationCode;
        char msg[64];
        strncpy(msg, g_savedActivationData.activationMsg, sizeof(msg) - 1);
        msg[sizeof(msg) - 1] = '\0';

        static int last_rendered_code = -1;
        static char last_rendered_msg[sizeof(msg)] = {0};

        if (g_showing_activation &&
            code == last_rendered_code &&
            strcmp(msg, last_rendered_msg) == 0)
        {
            Log.infoln("[app_view] show_activation: same code already displayed, skip refresh. code=%d", code);
            return;
        }

        stop_startup_sequence_task(true);
        g_showing_activation = false;

        Log.infoln("[app_view] show_activation: rendering activation page, code=%d, msg=%s", code, msg);

        if (usesFullActivationPage())
        {
            displayActivationCodePage(code, msg);
        }
        else
        {
            diy_displayActivationCodePage(code, msg);
        }
        last_rendered_code = code;
        strncpy(last_rendered_msg, msg, sizeof(last_rendered_msg) - 1);
        last_rendered_msg[sizeof(last_rendered_msg) - 1] = '\0';
        g_showing_activation = true;
    }

    void show_image()
    {
        stop_startup_sequence_task(true);
        g_showing_activation = false;
        if (app_wifi_is_provisioning())
        {
            app_user_action_finish_refresh();
            return;
        }

        app_gallery_image_ref_t image_ref = {};
        if (!app_gallery_current(&image_ref))
        {
            Log.warningln("[app_view] No playable image available.");
            app_user_action_finish_refresh();
            esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_WAITING, NULL, 1, portMAX_DELAY);
            return;
        }

        Log.infoln("[app_view] show image: id=%s order=%d dynamic=%d file=%s",
                   image_ref.id,
                   image_ref.order,
                   image_ref.is_dynamic ? 1 : 0,
                   image_ref.filename);
        g_current_image_name = image_ref.filename;

        if (!image_ref.is_dynamic && g_current_image_name.equals(g_last_image_name))
        {
            Log.infoln("[app_view] Skipping duplicate refresh for image: %s", g_current_image_name.c_str());
        }
        else
        {
            g_last_image_name = g_current_image_name;
            imgBufferMutex_lock();
            {
                HAL::SharedSpiLock spiLock;
                if (usesDownOrientation())
                {
                    display().setRotation(screen_rotation_map(3));
                }

                drawImageFromFileStream(0,
                                        0,
                                        HAL::GetHAL().screenIsColor(),
                                        true,
                                        g_current_image_name.c_str(),
                                        image_ref.is_sd,
                                        image_ref.is_dynamic);
            }

            imgBufferMutex_unlock();
        }

        app_user_action_finish_refresh();
        if (GetDeepSleepEnabled()) StartDeepSleepTimer();
    }

    void show_startup()
    {
        g_showing_activation = false;
        HAL::SharedSpiLock spiLock;
#if defined(USE_MUTIGRAY_EPAPER)
        display().deinitGrayMode();
#endif
        switch (screenCombo())
        {
        case 520:
        case 521:
        case 509:
        case 502:
        case 515:
        case 516:
        case 506:
            displayHomeUI(qr_code_bitmap, qr_code_wifi, 160, 160, g_savedApName);
            return;
        case 522:
        case 523:
        case 511:
            displayHomeUI(qr_code_wiki_260, qr_code_wifi_260, 260, 260, g_savedApName);
            return;
        case 510:
            displayHomeUI(qr_code_wiki_ee02, qr_code_wifi_260, 260, 260, g_savedApName);
            return;
        default:
            break;
        }

        if (isXiaoDiyKit())
        {
            const uint8_t orientation_index = screen_ui_orientation_index();
            display().setRotation(screen_rotation_map(orientation_index));
        }

        stop_startup_sequence_task(true);
        xTaskCreate(startup_sequence_task, "StartupUI", 2048, nullptr, tskIDLE_PRIORITY + 1, &g_startup_sequence_task);
    }

    void show_waiting()
    {
        stop_startup_sequence_task(true);
        g_showing_activation = false;
        const uint16_t fg_color = TFT_WHITE;
        const uint16_t bg_color = HAL::GetHAL().screenIsColor() ? TFT_RED : TFT_BLACK;

        HAL::SharedSpiLock spiLock;
        if (isXiaoDiyKit())
        {
            const uint8_t orientation_index = screen_ui_orientation_index();
            display().setRotation(screen_rotation_map(orientation_index));
        }
        screen_assets::drawWaitingScreen(display(), fg_color, bg_color);
        display().setRotation(screen_rotation_map(0));
    }

    struct LowBatteryTextMetrics
    {
        int16_t width = 0;
        int16_t height = 0;
    };

    LowBatteryTextMetrics measure_low_battery_text(const char *text, uint16_t font_size)
    {
        FT_BBox box = render.calculateBoundingBox(0, 0, font_size, Align::TopLeft, Layout::Horizontal, text);
        LowBatteryTextMetrics metrics;
        metrics.width = static_cast<int16_t>(box.xMax - box.xMin);
        metrics.height = static_cast<int16_t>(box.yMax - box.yMin);
        return metrics;
    }

    void draw_centered_low_battery_text(const char *text, uint16_t font_size, int16_t top, uint16_t fg_color, uint16_t bg_color)
    {
        LowBatteryTextMetrics metrics = measure_low_battery_text(text, font_size);
        int16_t x = (display().width() - metrics.width) / 2;
        if (x < 0)
        {
            x = 0;
        }

        render.setFontSize(font_size);
        render.setFontColor(fg_color, bg_color);
        render.setBackgroundFillMethod(BgFillMethod::None);
        render.setAlignment(Align::TopLeft);
        render.setLayout(Layout::Horizontal);
        render.drawString(text, x, top, fg_color, bg_color, Layout::Horizontal);
    }

    void draw_low_battery_icon(int16_t center_x, int16_t top, int16_t body_w, int16_t body_h, uint16_t fg_color, uint16_t bg_color)
    {
        int16_t body_x = center_x - body_w / 2;
        int16_t terminal_w = std::max<int16_t>(6, body_w / 10);
        int16_t terminal_h = std::max<int16_t>(12, body_h / 3);
        int16_t terminal_x = body_x + body_w;
        int16_t terminal_y = top + (body_h - terminal_h) / 2;

        display().fillRect(body_x - 2, top - 2, body_w + terminal_w + 6, body_h + 4, bg_color);
        for (int16_t i = 0; i < 3; ++i)
        {
            display().drawRoundRect(body_x + i, top + i, body_w - i * 2, body_h - i * 2, 6, fg_color);
        }
        display().fillRoundRect(terminal_x, terminal_y, terminal_w, terminal_h, 3, fg_color);

        int16_t fill_w = std::max<int16_t>(4, body_w / 12);
        int16_t fill_h = body_h - 16;
        if (fill_h < 4)
        {
            fill_h = body_h / 2;
        }
        display().fillRect(body_x + 8, top + (body_h - fill_h) / 2, fill_w, fill_h, fg_color);
    }

    void show_low_battery()
    {
        stop_startup_sequence_task(true);
        g_showing_activation = false;

        HAL::SharedSpiLock spiLock;
        if (isXiaoDiyKit())
        {
            const uint8_t orientation_index = screen_ui_orientation_index();
            display().setRotation(screen_rotation_map(orientation_index));
        }

#if defined(USE_MUTIGRAY_EPAPER)
        display().deinitGrayMode();
#endif

        const uint16_t bg_color = TFT_WHITE;
        const uint16_t fg_color = TFT_BLACK;
        const uint16_t accent_color = HAL::GetHAL().screenIsColor() ? TFT_RED : TFT_BLACK;

        int16_t screen_w = display().width();
        int16_t screen_h = display().height();
        int16_t short_side = std::min(screen_w, screen_h);
        int16_t battery_w = short_side * 44 / 100;
        int16_t battery_h = battery_w * 48 / 100;
        uint16_t title_font_size = std::max<uint16_t>(20, short_side * 8 / 100);
        uint16_t hint_font_size = std::max<uint16_t>(14, short_side * 5 / 100);
        int16_t icon_title_gap = short_side * 8 / 100;
        int16_t title_hint_gap = short_side * 3 / 100;

        EnsureRenderFontLoaded(robotovariablefont_wdthwght_ttf_data, sizeof(robotovariablefont_wdthwght_ttf_data));
        LowBatteryTextMetrics title_metrics = measure_low_battery_text("Battery low", title_font_size);
        LowBatteryTextMetrics hint_metrics = measure_low_battery_text("Please charge the device", hint_font_size);
        int16_t content_h = battery_h + icon_title_gap + title_metrics.height + title_hint_gap + hint_metrics.height;
        int16_t content_top = (screen_h - content_h) / 2;
        if (content_top < 0)
        {
            content_top = 0;
        }

        int16_t title_top = content_top + battery_h + icon_title_gap;
        int16_t hint_top = title_top + title_metrics.height + title_hint_gap;

        display().fillRect(0, 0, screen_w, screen_h, bg_color);
        draw_low_battery_icon(screen_w / 2, content_top, battery_w, battery_h, accent_color, bg_color);
        draw_centered_low_battery_text("Battery low", title_font_size, title_top, fg_color, bg_color);
        draw_centered_low_battery_text("Please charge the device", hint_font_size, hint_top, fg_color, bg_color);

        display().update();
        display().setRotation(screen_rotation_map(0));
    }

    void show_error()
    {
        stop_startup_sequence_task(true);
        g_showing_activation = false;
        HAL::SharedSpiLock spiLock;
        if (isXiaoDiyKit())
        {
            const uint8_t orientation_index = screen_ui_orientation_index();
            display().setRotation(screen_rotation_map(orientation_index));
        }
        const uint16_t bgColor = TFT_WHITE;
        const uint16_t fgColor = TFT_BLACK;

        const char *message = (g_errorMessage[0] != '\0') ? g_errorMessage : "Failed to open file";

        display().fillRect(0, 0, display().width(), display().height(), bgColor);

        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setBackgroundColor(bgColor);
        u8g2Fonts.setForegroundColor(fgColor);
        u8g2Fonts.setFont(u8g2_font_helvR18_tf);

        int16_t textWidth = u8g2Fonts.getUTF8Width(message);
        int16_t textHeight = u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent();
        int16_t x = (display().width() - textWidth) / 2;
        int16_t y = (display().height() + textHeight) / 2;

        u8g2Fonts.setCursor(x, y);
        u8g2Fonts.print(message);
        display().update();
        display().setRotation(screen_rotation_map(0));

        g_errorMessage[0] = '\0';
        StartDeepSleepTimer();
    }

    void view_task()
    {
        EventBits_t bits = xEventGroupWaitBits(
            viewEventGroup,
            VIEW_EVT_SHOW_ACTIVATION |
            VIEW_EVT_SHOW_IMAGE |
            VIEW_EVT_SHOW_FAST |
            VIEW_EVT_SHOW_STARTUP |
            VIEW_EVT_SHOW_WAIT |
            VIEW_EVT_SHOW_DOWNLOAD |
            VIEW_EVT_SHOW_CLEAR |
            VIEW_EVT_SHOW_ERROR |
            VIEW_EVT_SHOW_LOW_BATTERY,
            pdTRUE,
            pdFALSE,
            0);

        if (bits == 0)
        {
            return;
        }

        const bool has_display_power_work = (bits & VIEW_EVT_DISPLAY_POWER_MASK) != 0;
        const uint32_t display_power_serial = has_display_power_work ? power_serial() : 0;
        const bool has_refresh_state_work = (bits & VIEW_EVT_REFRESH_STATE_MASK) != 0;
        const uint32_t view_refresh_serial = has_refresh_state_work ? refresh_serial() : 0;

        if (bits & VIEW_EVT_SHOW_ACTIVATION)
        {
            show_activation();
        }

        if (bits & (VIEW_EVT_SHOW_IMAGE | VIEW_EVT_SHOW_FAST))
        {
            show_image();
        }

        if (bits & VIEW_EVT_SHOW_STARTUP)
        {
            show_startup();
        }

        if (bits & VIEW_EVT_SHOW_WAIT)
        {
            show_waiting();
        }

        if (bits & VIEW_EVT_SHOW_DOWNLOAD)
        {
            g_showing_activation = false;
        }

        if (bits & VIEW_EVT_SHOW_CLEAR)
        {
            g_showing_activation = false;
            HAL::SharedSpiLock spiLock;
            display().fillRect(0, 0, display().width(), display().height(), TFT_WHITE);
            display().update();
            if (!get_portal_status())
            {
                StartDeepSleepTimer();
            }
        }

        if (bits & VIEW_EVT_SHOW_ERROR)
        {
            show_error();
        }

        if (bits & VIEW_EVT_SHOW_LOW_BATTERY)
        {
            show_low_battery();
        }

        if (has_display_power_work)
        {
            release_display_power(display_power_serial);
        }

        if (has_refresh_state_work)
        {
            finish_refresh(view_refresh_serial);
        }
    }
}



void app_view_init()
{
    Log.infoln("[app_view] app view initialized");
    if (g_view_task_registered)
    {
        Log.warningln("[app_view] View task already running.");
        return;
    }
    init_fonts();

    imageBufferMutex = xSemaphoreCreateMutex();
    viewEventGroup = xEventGroupCreate();

    delay(1000);

    ESP_ERROR_CHECK(esp_event_handler_register(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_ACTIVATION_CODE, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_STARTUP, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_WAITING, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_DOWNLOADING, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_CLEAR, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_ERROR, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_LOW_BATTERY, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_INPUT_EVENT_BASE, APP_INPUT_EVENT_NEXT_IMAGE, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_INPUT_EVENT_BASE, APP_INPUT_EVENT_PREV_IMAGE, &view_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_INPUT_EVENT_BASE, APP_INPUT_EVENT_CLEAR_SCREEN, &view_event_handler, NULL));

    bool showedInitialCover = false;
    {
        HAL::SharedSpiLock spiLock;
        showedInitialCover = screen_assets::drawInitialCover(display(), GetBindState());
    }
    if (showedInitialCover)
    {
        delay(3000);
    }

    if (!MiniRT::addTask(view_task, 10, MiniRT::Priority::Low))
    {
        Log.errorln("[app_view] Failed to register view task with MiniRT");
        return;
    }
    g_view_task_registered = true;
}

bool app_view_is_showing_activation()
{
    return g_showing_activation;
}

bool app_view_is_refreshing()
{
    xSemaphoreTake(g_powerMutex, portMAX_DELAY);
    bool refreshing = g_refreshing;
    xSemaphoreGive(g_powerMutex);

    return refreshing;
}
