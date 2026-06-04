// GxEPD2 driver class for the E Ink ED103TC2 (10.3", 1872 x 1404,
// 16-level grayscale) panel as wired on the Seeed Studio reTerminal E1003
// (BOARD_SCREEN_COMBO 522).
//
// The panel is driven by an IT8951 controller. GxEPD2 ships an IT8951
// driver under src/it8951/ (GxEPD2_it103_1872x1404), but it has two
// problems that prevent it from working on the reTerminal:
//   1. It hardcodes the global `SPI` object everywhere instead of using
//      the inherited `_pSPIx` pointer, so `display.epd2.selectSPI(hspi,...)`
//      is silently ignored.
//   2. It is tuned for the Waveshare ES103TC1 (VCOM = -2.33 V, big-endian
//      image load, no 1bpp display mode setup, no X mirror). The Seeed
//      ED103TC2 needs VCOM = -1.40 V, little-endian load, the IT8951
//      1bpp display path (UP1SR + BGVR registers + display mode 1) and
//      an X coordinate flip.
//
// This file keeps the upstream library untouched: it provides an
// alternative driver in the example folder. Move the .h / .cpp pair next
// to your sketch to reuse it elsewhere.
//
// Reference (working implementation):
//   Seeed_GFX/Extensions/Tcon.cpp        - IT8951 command sequences
//   Seeed_GFX/TFT_Drivers/IT8951_Defines.h
//   Seeed_GFX/TFT_Drivers/ED103TC2_Init.h
//
// Memory note: we keep a 1bpp full-screen framebuffer (1872 * 1404 / 8 =
// 328 536 bytes, ~321 kB) in PSRAM. PSRAM is mandatory: select
// `Tools > PSRAM: OPI PSRAM` on XIAO_ESP32S3.

#ifndef _GxEPD2_ED103TC2_1872x1404_H_
#define _GxEPD2_ED103TC2_1872x1404_H_

#include <Arduino.h>
#include <GxEPD2_EPD.h>

class GxEPD2_ED103TC2_1872x1404 : public GxEPD2_EPD
{
  public:
    static const uint16_t WIDTH = 1872;
    static const uint16_t WIDTH_VISIBLE = WIDTH;
    static const uint16_t HEIGHT = 1404;
    // No bespoke enum exists in GxEPD2.h for ED103TC2. ES103TC1 is the
    // closest match (also 10.3" 1872x1404 IT8951) and GxEPD2_BW does not
    // branch on this value, so it is safe to reuse.
    static const GxEPD2::Panel panel = GxEPD2::ES103TC1;
    static const bool hasColor = false;
    static const bool hasPartialUpdate = true;
    static const bool hasFastPartialUpdate = false;
    static const uint16_t power_on_time = 100;       // ms
    static const uint16_t power_off_time = 100;      // ms
    static const uint16_t full_refresh_time = 3000;  // ms
    static const uint16_t partial_refresh_time = 800; // ms

    GxEPD2_ED103TC2_1872x1404(int16_t cs, int16_t dc, int16_t rst, int16_t busy);

    void init(uint32_t serial_diag_bitrate = 0) override;
    void init(uint32_t serial_diag_bitrate, bool initial,
              uint16_t reset_duration = 10, bool pulldown_rst_mode = false) override;

    void clearScreen(uint8_t value = 0xFF) override;
    void writeScreenBuffer(uint8_t value = 0xFF) override;

    void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h,
                    bool invert = false, bool mirror_y = false, bool pgm = false) override;
    void writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part,
                        int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h,
                        bool invert = false, bool mirror_y = false, bool pgm = false) override;
    // Below are not virtual in GxEPD2_EPD; they are conventional helpers
    // expected by the GxEPD2_BW / GxEPD2_3C / etc. wrappers via name lookup.
    void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y,
                    int16_t w, int16_t h, bool invert = false, bool mirror_y = false,
                    bool pgm = false);
    void writeImagePart(const uint8_t* black, const uint8_t* color,
                        int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h,
                        bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeNative(const uint8_t* data1, const uint8_t* data2,
                     int16_t x, int16_t y, int16_t w, int16_t h,
                     bool invert = false, bool mirror_y = false, bool pgm = false);

    void drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h,
                   bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part,
                       int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h,
                       bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImage(const uint8_t* black, const uint8_t* color,
                   int16_t x, int16_t y, int16_t w, int16_t h,
                   bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImagePart(const uint8_t* black, const uint8_t* color,
                       int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h,
                       bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawNative(const uint8_t* data1, const uint8_t* data2,
                    int16_t x, int16_t y, int16_t w, int16_t h,
                    bool invert = false, bool mirror_y = false, bool pgm = false);

    void refresh(bool partial_update_mode = false) override;
    void refresh(int16_t x, int16_t y, int16_t w, int16_t h) override;
    void powerOff() override;
    void hibernate() override;

  private:
    // Local 1bpp framebuffer; bit MSB = leftmost pixel, 1 = white, 0 = black
    // (matches GxEPD2_BW convention).
    static constexpr uint32_t WB = WIDTH / 8;            // 234
    static constexpr uint32_t FRAME_BYTES = uint32_t(WB) * HEIGHT; // 328 536

    uint8_t* _frame_buf;          // PSRAM allocated
    bool _frame_buf_dirty;

    // IT8951 device info (populated at init())
    uint16_t _panelW;
    uint16_t _panelH;
    uint32_t _imgBufAddr;

    void _allocFrameBuffer();
    void _fillFrameBuffer(uint8_t value);
    void _blitInto(const uint8_t* bitmap, int16_t x_part, int16_t y_part,
                   int16_t w_bitmap, int16_t h_bitmap,
                   int16_t x, int16_t y, int16_t w, int16_t h,
                   bool invert, bool mirror_y, bool pgm);

    // IT8951 SPI helpers (use _pSPIx so selectSPI() is honoured)
    void _waitForHRDY();
    void _waitForLUT();
    uint16_t _xfer16(uint16_t v);
    void _writeCmd16(uint16_t cmd);
    void _writeData16(uint16_t data);
    uint16_t _readData16();
    void _readData16N(uint16_t* buf, uint32_t n);
    void _writeReg(uint16_t reg, uint16_t value);
    uint16_t _readReg(uint16_t reg);
    void _setVCOM(uint16_t mV);

    // Higher-level IT8951 operations
    void _readDevInfo();
    void _enablePackedMode();
    void _systemRun();
    void _standby();
    void _sleep();
    void _setImgBufBaseAddr();  // write LISAR register before image load
    void _ld_img_area_1bpp(uint16_t x, uint16_t y, uint16_t w_pixels, uint16_t h_pixels);
    void _ld_img_end();
    void _dpy_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t mode);
    void _dpy_area_1bpp(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        uint16_t mode, uint8_t bg, uint8_t fg);

    void _powerOnIT8951();
    void _powerOffIT8951();
    void _dumpFrameToIT8951();   // upload _frame_buf to IT8951 image buffer
    void _doFullRefresh();
};

#endif // _GxEPD2_ED103TC2_1872x1404_H_
