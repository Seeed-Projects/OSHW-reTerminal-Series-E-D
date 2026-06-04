// See GxEPD2_ED103TC2_1872x1404.h for background on why this exists.
//
// Command sequences mirror Seeed_GFX/Extensions/Tcon.cpp + ED103TC2_Init.h
// but use the GxEPD2_EPD `_pSPIx` pointer so callers can pick the SPI bus.

#include "GxEPD2_ED103TC2_1872x1404.h"

// Diagnostic output: reTerminal E1003 serial monitor is wired to UART0.
// On ESP32-S3 with "USB CDC On Boot: Enabled", Serial = USB CDC (wrong
// port), Serial0 = UART0 (correct port where ROM bootloader also prints).
#define DBG Serial0

// ---------- IT8951 command / register addresses (from datasheet) ----------
#define IT8951_TCON_SYS_RUN        0x0001
#define IT8951_TCON_STANDBY        0x0002
#define IT8951_TCON_SLEEP          0x0003
#define IT8951_TCON_REG_RD         0x0010
#define IT8951_TCON_REG_WR         0x0011
#define IT8951_TCON_LD_IMG         0x0020
#define IT8951_TCON_LD_IMG_AREA    0x0021
#define IT8951_TCON_LD_IMG_END     0x0022
#define USDEF_I80_CMD_DPY_AREA     0x0034
#define USDEF_I80_CMD_GET_DEV_INFO 0x0302
#define USDEF_I80_CMD_VCOM         0x0039

#define IT8951_ROTATE_0     0
#define IT8951_2BPP         0
#define IT8951_3BPP         1
#define IT8951_4BPP         2
#define IT8951_8BPP         3
#define IT8951_LDIMG_L_ENDIAN  0
#define IT8951_LDIMG_B_ENDIAN  1

#define SYS_REG_BASE      0x0000
#define I80CPCR           (SYS_REG_BASE + 0x04)

#define DISPLAY_REG_BASE  0x1000
#define UP1SR             (DISPLAY_REG_BASE + 0x138)
#define LUTAFSR           (DISPLAY_REG_BASE + 0x224)
#define BGVR              (DISPLAY_REG_BASE + 0x250)

#define MCSR_BASE_ADDR    0x0200
#define LISAR             (MCSR_BASE_ADDR + 0x0008)

// VCOM for Seeed reTerminal E1003 (ED103TC2). Different from the Waveshare
// ES103TC1 the upstream driver targets (-2.33 V).
static constexpr uint16_t SEEED_E1003_VCOM_mV = 1400;

// Device info structure returned by USDEF_I80_CMD_GET_DEV_INFO.
struct IT8951DevInfo
{
    uint16_t usPanelW;
    uint16_t usPanelH;
    uint16_t usImgBufAddrL;
    uint16_t usImgBufAddrH;
    uint16_t usFWVersion[8];
    uint16_t usLUTVersion[8];
};

GxEPD2_ED103TC2_1872x1404::GxEPD2_ED103TC2_1872x1404(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
    GxEPD2_EPD(cs, dc, rst, busy,
               /*busy_level=*/LOW,      // HRDY = LOW means "not ready", we wait while pin reads LOW
               /*busy_timeout=*/2000000, // 2 s; surface hangs early rather than blocking 10 s
               WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate),
    _frame_buf(nullptr), _frame_buf_dirty(false),
    _panelW(WIDTH), _panelH(HEIGHT), _imgBufAddr(0)
{
}

// =====================================================================
// init
// =====================================================================
void GxEPD2_ED103TC2_1872x1404::init(uint32_t serial_diag_bitrate)
{
    init(serial_diag_bitrate, true, 10, false);
}

void GxEPD2_ED103TC2_1872x1404::init(uint32_t serial_diag_bitrate, bool initial,
                                     uint16_t reset_duration, bool pulldown_rst_mode)
{
    // Base class handles RST pulse, _pSPIx->begin(), pin modes. It also
    // toggles the reset line according to reset_duration.
    GxEPD2_EPD::init(serial_diag_bitrate, initial, reset_duration, pulldown_rst_mode);

    _allocFrameBuffer();
    _fillFrameBuffer(0xFF);

    if (_rst >= 0)
    {
        digitalWrite(_rst, LOW);  delay(10);
        digitalWrite(_rst, HIGH); delay(10);
    }
    _waitForHRDY();

    _setVCOM(SEEED_E1003_VCOM_mV);
    _readDevInfo();

    // Temperature must be set for IT8951 waveform selection.
    // Without this the controller accepts DPY_AREA but skips the refresh.
    _writeCmd16(0x0040);
    _writeData16(0x0001);
    _writeData16(16);

    _power_is_on = true;

    DBG.print(F("[ED103TC2] init OK  Panel="));
    DBG.print(_panelW); DBG.print('x'); DBG.print(_panelH);
    DBG.print(F("  imgBuf=0x")); DBG.println(_imgBufAddr, HEX);
}

// =====================================================================
// Framebuffer
// =====================================================================
void GxEPD2_ED103TC2_1872x1404::_allocFrameBuffer()
{
    if (_frame_buf) return;
#if defined(ESP32)
    _frame_buf = (uint8_t*) ps_malloc(FRAME_BYTES);
    if (!_frame_buf)
    {
        // Fall back to internal RAM (will likely fail at 321 kB on ESP32-S3
        // without PSRAM, but try anyway so user sees a clear error path).
        _frame_buf = (uint8_t*) malloc(FRAME_BYTES);
    }
#else
    _frame_buf = (uint8_t*) malloc(FRAME_BYTES);
#endif
    if (!_frame_buf && _diag_enabled)
    {
        DBG.println(F("[ED103TC2] PSRAM allocation failed (~321 kB)."));
    }
}

void GxEPD2_ED103TC2_1872x1404::_fillFrameBuffer(uint8_t value)
{
    if (!_frame_buf) return;
    memset(_frame_buf, value, FRAME_BYTES);
    _frame_buf_dirty = true;
}

void GxEPD2_ED103TC2_1872x1404::_blitInto(const uint8_t* bitmap,
                                          int16_t x_part, int16_t y_part,
                                          int16_t w_bitmap, int16_t h_bitmap,
                                          int16_t x, int16_t y, int16_t w, int16_t h,
                                          bool invert, bool mirror_y, bool pgm)
{
    if (!_frame_buf || !bitmap) return;
    if (w <= 0 || h <= 0) return;

    int16_t wb_bitmap = (w_bitmap + 7) / 8;

    // Snap x and x_part to 8-pixel boundary like other GxEPD2 drivers.
    x_part -= x_part % 8;
    x      -= x % 8;
    w       = 8 * ((w + 7) / 8);

    int16_t x1 = x < 0 ? 0 : x;
    int16_t y1 = y < 0 ? 0 : y;
    int16_t w1 = (x + w) <= int16_t(WIDTH)  ? w : int16_t(WIDTH)  - x;
    int16_t h1 = (y + h) <= int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y;
    int16_t dx = x1 - x;
    int16_t dy = y1 - y;
    w1 -= dx;
    h1 -= dy;
    if (w1 <= 0 || h1 <= 0) return;

    for (int16_t i = 0; i < h1; i++)
    {
        for (int16_t j = 0; j < w1 / 8; j++)
        {
            int16_t bx = x_part / 8 + j + dx / 8;
            int16_t by = mirror_y ? (h_bitmap - 1 - (y_part + i + dy))
                                  : (y_part + i + dy);
            if (bx < 0 || bx >= wb_bitmap || by < 0 || by >= h_bitmap) continue;
            uint32_t idx = uint32_t(bx) + uint32_t(by) * uint32_t(wb_bitmap);
            uint8_t data;
            if (pgm)
            {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
                data = pgm_read_byte(&bitmap[idx]);
#else
                data = bitmap[idx];
#endif
            }
            else
            {
                data = bitmap[idx];
            }
            if (invert) data = ~data;
            uint32_t fb_idx = uint32_t((x1 + j * 8) / 8)
                            + uint32_t(y1 + i) * uint32_t(WB);
            _frame_buf[fb_idx] = data;
        }
#if defined(ESP8266) || defined(ESP32)
        if ((i & 0x3F) == 0) yield();
#endif
    }
    _frame_buf_dirty = true;
}

// =====================================================================
// GxEPD2_EPD overrides
// =====================================================================
void GxEPD2_ED103TC2_1872x1404::clearScreen(uint8_t value)
{
    _fillFrameBuffer(value);
    _doFullRefresh();
    _initial_write = false;
    _initial_refresh = false;
}

void GxEPD2_ED103TC2_1872x1404::writeScreenBuffer(uint8_t value)
{
    _fillFrameBuffer(value);
}

void GxEPD2_ED103TC2_1872x1404::writeImage(const uint8_t bitmap[],
                                           int16_t x, int16_t y, int16_t w, int16_t h,
                                           bool invert, bool mirror_y, bool pgm)
{
    _blitInto(bitmap, 0, 0, w, h, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_ED103TC2_1872x1404::writeImagePart(const uint8_t bitmap[],
                                               int16_t x_part, int16_t y_part,
                                               int16_t w_bitmap, int16_t h_bitmap,
                                               int16_t x, int16_t y, int16_t w, int16_t h,
                                               bool invert, bool mirror_y, bool pgm)
{
    _blitInto(bitmap, x_part, y_part, w_bitmap, h_bitmap,
              x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_ED103TC2_1872x1404::writeImage(const uint8_t* black, const uint8_t* /*color*/,
                                           int16_t x, int16_t y, int16_t w, int16_t h,
                                           bool invert, bool mirror_y, bool pgm)
{
    if (black) writeImage(black, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_ED103TC2_1872x1404::writeImagePart(const uint8_t* black, const uint8_t* /*color*/,
                                               int16_t x_part, int16_t y_part,
                                               int16_t w_bitmap, int16_t h_bitmap,
                                               int16_t x, int16_t y, int16_t w, int16_t h,
                                               bool invert, bool mirror_y, bool pgm)
{
    if (black) writeImagePart(black, x_part, y_part, w_bitmap, h_bitmap,
                              x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_ED103TC2_1872x1404::writeNative(const uint8_t* data1, const uint8_t* /*data2*/,
                                            int16_t x, int16_t y, int16_t w, int16_t h,
                                            bool invert, bool mirror_y, bool pgm)
{
    // GxEPD2_BW does not normally call writeNative; treat it like writeImage.
    if (data1) writeImage(data1, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_ED103TC2_1872x1404::drawImage(const uint8_t bitmap[],
                                          int16_t x, int16_t y, int16_t w, int16_t h,
                                          bool invert, bool mirror_y, bool pgm)
{
    writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
    refresh(x, y, w, h);
}

void GxEPD2_ED103TC2_1872x1404::drawImagePart(const uint8_t bitmap[],
                                              int16_t x_part, int16_t y_part,
                                              int16_t w_bitmap, int16_t h_bitmap,
                                              int16_t x, int16_t y, int16_t w, int16_t h,
                                              bool invert, bool mirror_y, bool pgm)
{
    writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap,
                   x, y, w, h, invert, mirror_y, pgm);
    refresh(x, y, w, h);
}

void GxEPD2_ED103TC2_1872x1404::drawImage(const uint8_t* black, const uint8_t* color,
                                          int16_t x, int16_t y, int16_t w, int16_t h,
                                          bool invert, bool mirror_y, bool pgm)
{
    writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
    refresh(x, y, w, h);
}

void GxEPD2_ED103TC2_1872x1404::drawImagePart(const uint8_t* black, const uint8_t* color,
                                              int16_t x_part, int16_t y_part,
                                              int16_t w_bitmap, int16_t h_bitmap,
                                              int16_t x, int16_t y, int16_t w, int16_t h,
                                              bool invert, bool mirror_y, bool pgm)
{
    writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap,
                   x, y, w, h, invert, mirror_y, pgm);
    refresh(x, y, w, h);
}

void GxEPD2_ED103TC2_1872x1404::drawNative(const uint8_t* data1, const uint8_t* data2,
                                           int16_t x, int16_t y, int16_t w, int16_t h,
                                           bool invert, bool mirror_y, bool pgm)
{
    writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
    refresh(x, y, w, h);
}

void GxEPD2_ED103TC2_1872x1404::refresh(bool /*partial_update_mode*/)
{
    // The IT8951 + ED103TC2 needs the data uploaded each time anyway.
    _doFullRefresh();
}

void GxEPD2_ED103TC2_1872x1404::refresh(int16_t /*x*/, int16_t /*y*/,
                                        int16_t /*w*/, int16_t /*h*/)
{
    _doFullRefresh();
}

void GxEPD2_ED103TC2_1872x1404::powerOff()
{
    _powerOffIT8951();
}

void GxEPD2_ED103TC2_1872x1404::hibernate()
{
    _powerOffIT8951();
}

// =====================================================================
// IT8951 SPI primitives
// =====================================================================

void GxEPD2_ED103TC2_1872x1404::_waitForHRDY()
{
    if (_busy < 0) { delay(1); return; }
    unsigned long start = micros();
    // HRDY HIGH means ready.
    while (digitalRead(_busy) == LOW)
    {
        if (micros() - start > _busy_timeout)
        {
            if (_diag_enabled) DBG.println(F("[ED103TC2] HRDY timeout"));
            break;
        }
#if defined(ESP8266) || defined(ESP32)
        yield();
#endif
    }
}

void GxEPD2_ED103TC2_1872x1404::_waitForLUT()
{
    // Loop reading LUTAFSR until it returns 0 (all LUT engines idle).
    unsigned long start = millis();
    while (_readReg(LUTAFSR) != 0)
    {
        if (millis() - start > 30000)
        {
            if (_diag_enabled) DBG.println(F("[ED103TC2] LUT busy timeout"));
            break;
        }
#if defined(ESP8266) || defined(ESP32)
        yield();
#endif
    }
}

uint16_t GxEPD2_ED103TC2_1872x1404::_xfer16(uint16_t v)
{
    return _pSPIx->transfer16(v);
}

void GxEPD2_ED103TC2_1872x1404::_writeCmd16(uint16_t cmd)
{
    _pSPIx->beginTransaction(_spi_settings);
    if (_cs >= 0) digitalWrite(_cs, LOW);
    _waitForHRDY();
    _xfer16(0x6000);   // write-command preamble
    _waitForHRDY();
    _xfer16(cmd);
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _pSPIx->endTransaction();
}

void GxEPD2_ED103TC2_1872x1404::_writeData16(uint16_t data)
{
    _pSPIx->beginTransaction(_spi_settings);
    if (_cs >= 0) digitalWrite(_cs, LOW);
    _waitForHRDY();
    _xfer16(0x0000);   // write-data preamble
    _waitForHRDY();
    _xfer16(data);
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _pSPIx->endTransaction();
}

uint16_t GxEPD2_ED103TC2_1872x1404::_readData16()
{
    _pSPIx->beginTransaction(_spi_settings);
    if (_cs >= 0) digitalWrite(_cs, LOW);
    _waitForHRDY();
    _xfer16(0x1000);   // read-data preamble
    _waitForHRDY();
    (void)_xfer16(0);  // dummy
    _waitForHRDY();
    uint16_t v = _xfer16(0);
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _pSPIx->endTransaction();
    return v;
}

void GxEPD2_ED103TC2_1872x1404::_readData16N(uint16_t* buf, uint32_t n)
{
    _pSPIx->beginTransaction(_spi_settings);
    if (_cs >= 0) digitalWrite(_cs, LOW);
    _waitForHRDY();
    _xfer16(0x1000);
    _waitForHRDY();
    (void)_xfer16(0); // dummy
    _waitForHRDY();
    for (uint32_t i = 0; i < n; i++)
    {
        buf[i] = _xfer16(0);
    }
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _pSPIx->endTransaction();
}

void GxEPD2_ED103TC2_1872x1404::_writeReg(uint16_t reg, uint16_t value)
{
    _writeCmd16(IT8951_TCON_REG_WR);
    _writeData16(reg);
    _writeData16(value);
}

uint16_t GxEPD2_ED103TC2_1872x1404::_readReg(uint16_t reg)
{
    _writeCmd16(IT8951_TCON_REG_RD);
    _writeData16(reg);
    return _readData16();
}

void GxEPD2_ED103TC2_1872x1404::_setVCOM(uint16_t mV)
{
    _writeCmd16(USDEF_I80_CMD_VCOM);
    _writeData16(0x0002); // sub-command: set VCOM
    _writeData16(mV);
}

void GxEPD2_ED103TC2_1872x1404::_readDevInfo()
{
    _writeCmd16(USDEF_I80_CMD_GET_DEV_INFO);
    IT8951DevInfo info{};
    _readData16N(reinterpret_cast<uint16_t*>(&info),
                 sizeof(info) / sizeof(uint16_t));

    _panelW     = info.usPanelW;
    _panelH     = info.usPanelH;
    _imgBufAddr = uint32_t(info.usImgBufAddrL)
                | (uint32_t(info.usImgBufAddrH) << 16);

    if (_panelW == 0 || _panelH == 0) {
        DBG.println(F("[ED103TC2] ERROR: panel size = 0, IT8951 communication failed!"));
    }
}

void GxEPD2_ED103TC2_1872x1404::_enablePackedMode()
{
    _writeReg(I80CPCR, 0x0001);
}

void GxEPD2_ED103TC2_1872x1404::_systemRun() { _writeCmd16(IT8951_TCON_SYS_RUN); }
void GxEPD2_ED103TC2_1872x1404::_standby()   { _writeCmd16(IT8951_TCON_STANDBY); }
void GxEPD2_ED103TC2_1872x1404::_sleep()     { _writeCmd16(IT8951_TCON_SLEEP); }

void GxEPD2_ED103TC2_1872x1404::_setImgBufBaseAddr()
{
    // Tell IT8951 where in its internal SRAM to place incoming pixel data.
    // Seeed_GFX calls this before EVERY image load (tconSetImgBufBaseAddr).
    uint16_t addrH = uint16_t((_imgBufAddr >> 16) & 0xFFFF);
    uint16_t addrL = uint16_t(_imgBufAddr & 0xFFFF);
    _writeReg(LISAR + 2, addrH);
    _writeReg(LISAR,     addrL);
}

// LD_IMG_AREA for a 1bpp upload disguised as 8bpp - data width is reduced
// by /8. X coordinate is flipped to match Seeed's panel orientation.
void GxEPD2_ED103TC2_1872x1404::_ld_img_area_1bpp(uint16_t x, uint16_t y,
                                                   uint16_t w_pixels, uint16_t h_pixels)
{
    // X mirror, as Seeed's tconLoad1bppImage does.
    uint16_t xFlip = (_panelW - 1) - x - w_pixels + 1;

    uint16_t args[5];
    args[0] = (IT8951_LDIMG_L_ENDIAN << 8)
            | (IT8951_8BPP << 4)
            | IT8951_ROTATE_0;
    args[1] = (xFlip + 7) / 8;     // x in 8-pixel units (1bpp packed as 8bpp)
    args[2] = y;
    args[3] = (w_pixels + 7) / 8;  // width in 8-pixel units
    args[4] = h_pixels;

    _writeCmd16(IT8951_TCON_LD_IMG_AREA);
    for (int i = 0; i < 5; i++) _writeData16(args[i]);
}

void GxEPD2_ED103TC2_1872x1404::_ld_img_end()
{
    _writeCmd16(IT8951_TCON_LD_IMG_END);
}

void GxEPD2_ED103TC2_1872x1404::_dpy_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                          uint16_t mode)
{
    _writeCmd16(USDEF_I80_CMD_DPY_AREA);
    _writeData16(x);
    _writeData16(y);
    _writeData16(w);
    _writeData16(h);
    _writeData16(mode);
}

void GxEPD2_ED103TC2_1872x1404::_dpy_area_1bpp(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                                uint16_t mode, uint8_t bg, uint8_t fg)
{
    uint16_t xFlip = (_panelW - 1) - x - w + 1;

    // Enable 1bpp display mode (UP1SR bit 18 / "+2 bit 2")
    _writeReg(UP1SR + 2, _readReg(UP1SR + 2) | (1u << 2));
    // Background = bit 0 colour (high byte), foreground = bit 1 colour (low byte)
    _writeReg(BGVR, uint16_t(bg) << 8 | fg);

    _dpy_area(xFlip, y, w, h, mode);
    _waitForLUT();

    // Restore normal mode
    _writeReg(UP1SR + 2, _readReg(UP1SR + 2) & ~(1u << 2));
}

// =====================================================================
// Power & refresh
// =====================================================================
void GxEPD2_ED103TC2_1872x1404::_powerOnIT8951()
{
    if (_power_is_on) return;
    _systemRun();
    _waitForHRDY();
    _power_is_on = true;
}

void GxEPD2_ED103TC2_1872x1404::_powerOffIT8951()
{
    if (!_power_is_on) return;
    _standby();
    _power_is_on = false;
}

// Stream the full local framebuffer into IT8951's image buffer using the
// Seeed-compatible LD_IMG_AREA flow.
//
// IMPORTANT byte order:
//   In Seeed_GFX the framebuffer is treated as TWord* (uint16_t*) on a
//   little-endian host; source word k = (bytes[2k+1] << 8) | bytes[2k].
//   Their tconWirteNData sends each word MSB-first via spi.transfer16().
//   For "filp=false" they additionally reverse the WORD order across the
//   row, which combined with the MSB-first transfer of LE-assembled words
//   places bytes on the wire in pure right-to-left order:
//     bytes[2W-1], bytes[2W-2], ..., bytes[1], bytes[0]
//   To match exactly, we send the bytes in reverse order one at a time.
void GxEPD2_ED103TC2_1872x1404::_dumpFrameToIT8951()
{
    if (!_frame_buf) return;

    _powerOnIT8951();
    _setImgBufBaseAddr();
    _ld_img_area_1bpp(0, 0, WIDTH, HEIGHT);
    _pSPIx->beginTransaction(_spi_settings);
    if (_cs >= 0) digitalWrite(_cs, LOW);
    _waitForHRDY();
    _xfer16(0x0000); // write-data preamble
    _waitForHRDY();

    for (uint32_t row = 0; row < HEIGHT; row++)
    {
        const uint8_t* row_p = _frame_buf + row * WB;
        // Send the row bytes from rightmost (X = WIDTH-1) to leftmost
        // (X = 0). Each byte holds 8 horizontal pixels with the MSB being
        // the leftmost pixel within the byte. This pure-byte-reverse
        // order is what Seeed_GFX emits for the unflipped path.
        for (int32_t b = int32_t(WB) - 1; b >= 0; b--)
        {
            _pSPIx->transfer(row_p[b]);
        }
#if defined(ESP8266) || defined(ESP32)
        if ((row & 0x1F) == 0) yield();
#endif
    }

    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _pSPIx->endTransaction();

    _ld_img_end();
}

void GxEPD2_ED103TC2_1872x1404::_doFullRefresh()
{
    _powerOnIT8951();
    _setImgBufBaseAddr();

    // Temperature must be set before every display operation
    _writeCmd16(0x0040);
    _writeData16(0x0001);
    _writeData16(16);

    // Upload framebuffer as 8BPP (expand 1bpp → 8bpp, X-mirrored)
    {
        uint16_t args[5];
        args[0] = (IT8951_LDIMG_B_ENDIAN << 8) | (IT8951_8BPP << 4) | IT8951_ROTATE_0;
        args[1] = 0;
        args[2] = 0;
        args[3] = WIDTH;
        args[4] = HEIGHT;
        _writeCmd16(IT8951_TCON_LD_IMG_AREA);
        for (int i = 0; i < 5; i++) _writeData16(args[i]);
    }

    unsigned long t0 = millis();
    _pSPIx->beginTransaction(_spi_settings);
    if (_cs >= 0) digitalWrite(_cs, LOW);
    _waitForHRDY();
    _xfer16(0x0000);
    _waitForHRDY();

    const uint16_t WB = (WIDTH + 7) / 8;
    for (uint16_t row = 0; row < HEIGHT; row++)
    {
        const uint8_t* rp = _frame_buf + uint32_t(row) * WB;
        // X-mirror: ED103TC2 panel has reversed X axis.
        // Send bytes right-to-left, bits LSB-first within each byte.
        for (int16_t col = WB - 1; col >= 0; col--)
        {
            uint8_t byte_val = rp[col];
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                _pSPIx->transfer((byte_val & 0x01) ? 0xFF : 0x00);
                byte_val >>= 1;
            }
        }
        if ((row & 0x3F) == 0) yield();
    }

    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _pSPIx->endTransaction();

    _writeCmd16(IT8951_TCON_LD_IMG_END);
    _waitForHRDY();

    // DPY_AREA (GC16 full update)
    _writeCmd16(USDEF_I80_CMD_DPY_AREA);
    _waitForHRDY();
    _writeData16(0);
    _waitForHRDY();
    _writeData16(0);
    _waitForHRDY();
    _writeData16(WIDTH);
    _waitForHRDY();
    _writeData16(HEIGHT);
    _waitForHRDY();
    _writeData16(2);

    unsigned long t1 = millis();
    while (digitalRead(_busy) == LOW)
    {
        if (millis() - t1 > 15000) break;
        delay(100);
    }
    DBG.print(F("[ED103TC2] refresh "));
    DBG.print(millis() - t0);
    DBG.println(F(" ms"));

    _frame_buf_dirty = false;
    _initial_write = false;
    _initial_refresh = false;
}
