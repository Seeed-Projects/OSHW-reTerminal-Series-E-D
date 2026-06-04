#include <SPI.h>
#include <GxEPD2_7C.h>
#include "GxEPD2_T133A01_1200x1600.h"

// reTerminal serial monitor uses UART0.  With "USB CDC On Boot: Enabled",
// Serial = USB CDC, Serial0 = UART0 (the physical TX/RX pads on the board).
// All driver-level diagnostic output is sent through Serial0.
#define DBG Serial0
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

// ===== Pin mapping =====
#define EPD_SCK_PIN   7
#define EPD_MISO_PIN  8
#define EPD_MOSI_PIN  9
#define EPD_CS_PIN    10
#define EPD_DC_PIN    11
#define EPD_CS1_PIN   2
#define EPD_RES_PIN   38
#define EPD_BUSY_PIN  13
#define EPD_ENABLE_PIN 12

SPIClass hspi(HSPI);

// ===== Display: 13.3" 6-Color 1200x1600, dual-chip =====
#define MAX_DISPLAY_BUFFER_SIZE 24000u
#define MAX_HEIGHT(EPD) \
    (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) \
         ? EPD::HEIGHT \
         : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))

GxEPD2_7C<GxEPD2_T133A01_1200x1600, MAX_HEIGHT(GxEPD2_T133A01_1200x1600)>
    display(GxEPD2_T133A01_1200x1600(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN,
                                      EPD_BUSY_PIN, EPD_CS1_PIN, EPD_ENABLE_PIN));

// Color shorthand — T133A01 is a Spectra 6 panel.  Only these six
// colors are physically reproducible; the demo never references
// GxEPD_ORANGE because the panel has no native orange ink.
#define C_BLACK   GxEPD_BLACK
#define C_WHITE   GxEPD_WHITE
#define C_GREEN   GxEPD_GREEN
#define C_BLUE    GxEPD_BLUE
#define C_RED     GxEPD_RED
#define C_YELLOW  GxEPD_YELLOW

void setup()
{
  Serial.begin(115200);
  DBG.begin(115200);     // hardware UART0 (where the user's serial monitor is attached)
  delay(200);
  Serial.println(F("[E1004] GxEPD2 reTerminal E1004 Demo (13.3\" 6-Color)"));
  DBG.println(F("[E1004] GxEPD2 reTerminal E1004 Demo (13.3\" 6-Color)"));

  hspi.begin(EPD_SCK_PIN, EPD_MISO_PIN, EPD_MOSI_PIN, -1);
  // Match Seeed_GFX: 10 MHz SPI clock (XIAO_SPI_Frequency.h)
  display.epd2.selectSPI(hspi, SPISettings(10000000, MSBFIRST, SPI_MODE0));
  display.init(115200);  // enable diagnostic prints over Serial0/UART0

  // E1004 is a 13.3" 6-color panel — full refresh itself takes around 40s.
  // Keep each rendered page visible for at least 60s before moving on.
  const uint32_t PAGE_HOLD_MS = 60000;

  Serial.println(F("[E1004] Screen 1: Splash"));
  showSplashScreen();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1004] Screen 2: Color Palette"));
  showColorPalette();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1004] Screen 3: Typography"));
  showColorTypography();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1004] Screen 4: Geometry"));
  showColorGeometry();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1004] Screen 5: Patterns"));
  showColorPatterns();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1004] Screen 6: Dashboard"));
  showDashboard();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1004] Demo complete. Hibernating."));
  display.hibernate();
}

void loop() {}

// =====================================================================
void drawCenteredText(const char* text, int16_t y, const GFXfont* font)
{
  display.setFont(font);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor((display.width() - tbw) / 2 - tbx, y);
  display.print(text);
}

// =====================================================================
// Screen 1: Splash
// =====================================================================
void showSplashScreen()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(C_WHITE);

    // Colorful top band (5 panel colors, excluding white)
    uint16_t colors[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK};
    int stripeW = (W - 40) / 5;
    for (int i = 0; i < 5; i++)
      display.fillRect(20 + i * stripeW, 20, stripeW, 16, colors[i]);

    display.drawRect(20, 50, W - 40, H - 70, C_BLACK);
    display.drawRect(24, 54, W - 48, H - 78, C_BLACK);

    display.setTextColor(C_BLACK);
    drawCenteredText("reTerminal E1004", H / 2 - 120, &FreeSansBold24pt7b);

    display.setTextColor(C_RED);
    drawCenteredText("13.3\" 6-Color e-Paper", H / 2 - 60, &FreeSansBold18pt7b);

    display.drawFastHLine(W / 4, H / 2 - 20, W / 2, C_BLUE);

    display.setTextColor(C_GREEN);
    drawCenteredText("GxEPD2 + T133A01 Driver", H / 2 + 20, &FreeSansBold18pt7b);

    display.setTextColor(C_BLUE);
    drawCenteredText("1200 x 1600 | Dual-Chip | 6 Colors", H / 2 + 70, &FreeSansBold12pt7b);

    display.setTextColor(C_RED);
    drawCenteredText("PSRAM framebuffer | ~937 KB", H / 2 + 110, &FreeSans9pt7b);

    // Bottom colorful band (reverse order)
    for (int i = 0; i < 5; i++)
      display.fillRect(20 + i * stripeW, H - 36, stripeW, 16, colors[4 - i]);

    display.setTextColor(C_BLACK);
    drawCenteredText("Seeed Studio x GxEPD2", H - 50, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 2: Color Palette
// =====================================================================
void showColorPalette()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(C_WHITE);
    display.fillRect(0, 0, W, 50, C_BLACK);
    display.setTextColor(C_WHITE);
    drawCenteredText("6-Color Palette", 38, &FreeSansBold18pt7b);

    const uint16_t swatchColors[] = {C_BLACK, C_WHITE, C_RED, C_GREEN, C_BLUE, C_YELLOW};
    const char* names[] = {"Black", "White", "Red", "Green", "Blue", "Yellow"};
    int sw = 160, sh = 180, gap = 14;
    int sx = (W - 6 * sw - 5 * gap) / 2;
    int sy = 90;

    for (int i = 0; i < 6; i++) {
      int x = sx + i * (sw + gap);
      display.fillRoundRect(x, sy, sw, sh, 8, swatchColors[i]);
      display.drawRoundRect(x, sy, sw, sh, 8, C_BLACK);
      display.setFont(&FreeSansBold12pt7b);
      display.setTextColor(C_BLACK);
      int16_t tbx, tby; uint16_t tbw, tbh;
      display.getTextBounds(names[i], 0, 0, &tbx, &tby, &tbw, &tbh);
      display.setCursor(x + (sw - tbw) / 2 - tbx, sy + sh + 30);
      display.print(names[i]);
    }

    // Color combos (circles on colored backgrounds)
    int row2Y = sy + sh + 85;
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(C_BLACK);
    display.setCursor(sx, row2Y - 10);
    display.print("Color combinations:");

    int cx = sx + 20;
    uint16_t bgCols[] = {C_RED, C_GREEN, C_BLUE, C_YELLOW, C_BLACK};
    uint16_t fgCols[] = {C_YELLOW, C_RED, C_GREEN, C_BLUE, C_RED};
    for (int i = 0; i < 5; i++) {
      int x = cx + i * 180;
      display.fillRoundRect(x, row2Y + 10, 150, 120, 10, bgCols[i]);
      display.fillCircle(x + 75, row2Y + 70, 40, fgCols[i]);
    }

    // Full-width bars
    int barY = row2Y + 180;
    display.setTextColor(C_BLACK);
    display.setCursor(sx, barY - 10);
    display.print("Full-width color bars:");
    uint16_t barCols[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK};
    for (int i = 0; i < 5; i++)
      display.fillRect(sx, barY + 10 + i * 32, W - 2 * sx, 24, barCols[i]);

    // Large circle ring composition
    int ringY = barY + 240;
    display.setTextColor(C_BLACK);
    display.setCursor(sx, ringY - 10);
    display.print("Overlapping circles:");
    uint16_t ringColors[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK};
    for (int i = 0; i < 5; i++)
      display.fillCircle(sx + 100 + i * 200, ringY + 100, 80, ringColors[i]);

    drawCenteredText("All 6 colors on 13.3\" e-Paper", H - 30, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 3: Typography
// =====================================================================
void showColorTypography()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(C_WHITE);
    display.fillRect(0, 0, W, 50, C_BLUE);
    display.setTextColor(C_WHITE);
    drawCenteredText("Color Typography", 38, &FreeSansBold18pt7b);

    int y = 110, x = 50;

    // Large colored headings
    display.setTextColor(C_BLACK);
    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(x, y); display.print("Black 24pt");
    display.setTextColor(C_RED);
    display.setCursor(x + 420, y); display.print("Red 24pt");
    y += 75;

    display.setTextColor(C_GREEN);
    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(x, y); display.print("Green");
    display.setTextColor(C_BLUE);
    display.setCursor(x + 280, y); display.print("Blue");
    display.setTextColor(C_YELLOW);
    display.setCursor(x + 500, y); display.print("Yellow");
    y += 85;

    display.setTextColor(C_YELLOW);
    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(x, y); display.print("Yellow 18pt - warm sunshine");
    y += 72;

    display.drawFastHLine(x, y, W - 100, C_RED);
    y += 35;

    // Color text on color backgrounds
    display.setFont(&FreeSansBold12pt7b);
    uint16_t bgColors[] = {C_RED, C_GREEN, C_BLUE, C_YELLOW, C_BLACK, C_WHITE};
    const char* labels[] = {
      " White on Red ", " White on Green ", " White on Blue ",
      " Black on Yellow ", " Red on Black ", " Black on White "
    };
    uint16_t fgText[] = {C_WHITE, C_WHITE, C_WHITE, C_BLACK, C_RED, C_BLACK};
    for (int i = 0; i < 6; i++) {
      int bx = (i < 3) ? x : x + 500;
      int by = y + (i % 3) * 65;
      display.fillRoundRect(bx, by, 400, 45, 6, bgColors[i]);
      display.setTextColor(fgText[i]);
      display.setCursor(bx + 15, by + 32);
      display.print(labels[i]);
    }
    y += 240;

    // Right-side: black box with colored text
    int rbx = x, rby = y;
    display.fillRoundRect(rbx, rby, W - 100, 420, 10, C_BLACK);
    display.setFont(&FreeSansBold24pt7b);
    int ry = rby + 60;
    uint16_t rColors[] = {C_RED, C_GREEN, C_BLUE, C_YELLOW, C_WHITE};
    const char* rLabels[] = {"Red on Dark", "Green on Dark", "Blue on Dark",
                              "Yellow on Dark", "White on Dark"};
    for (int i = 0; i < 5; i++) {
      display.setTextColor(rColors[i]);
      display.setCursor(rbx + 30, ry);
      display.print(rLabels[i]);
      ry += 65;
    }

    display.setTextColor(C_BLACK);
    drawCenteredText("6-color text on 13.3\" panel", H - 30, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 4: Geometry
// =====================================================================
void showColorGeometry()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(C_WHITE);
    display.fillRect(0, 0, W, 50, C_GREEN);
    display.setTextColor(C_WHITE);
    drawCenteredText("Color Geometry", 38, &FreeSansBold18pt7b);
    display.setTextColor(C_BLACK);

    // Cascading colored rectangles
    uint16_t rColors[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK};
    for (int i = 0; i < 5; i++)
      display.fillRect(50 + i * 55, 80 + i * 15, 150, 90, rColors[i]);

    // Colored circles row
    uint16_t cColors[] = {C_BLUE, C_RED, C_GREEN, C_BLACK, C_YELLOW};
    for (int i = 0; i < 5; i++)
      display.fillCircle(600 + i * 100, 160, 40, cColors[i]);

    // Colored triangles
    int ty = 300;
    uint16_t triColors[] = {C_RED, C_GREEN, C_BLUE, C_YELLOW, C_BLACK};
    for (int i = 0; i < 5; i++) {
      int tx = 60 + i * 170;
      display.fillTriangle(tx, ty + 80, tx + 40, ty, tx + 80, ty + 80, triColors[i]);
    }

    // Olympic rings
    int ox = 250, oy = 500;
    uint16_t olyColors[] = {C_BLUE, C_BLACK, C_RED, C_YELLOW, C_GREEN};
    int olyX[] = {0, 90, 180, 45, 135};
    int olyY[] = {0, 0, 0, 50, 50};
    for (int i = 0; i < 5; i++)
      for (int r = 0; r < 5; r++)
        display.drawCircle(ox + olyX[i], oy + olyY[i], 40 - r, olyColors[i]);

    // Color mosaic (right side)
    int mx = 700, my = 450;
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(C_BLACK);
    display.setCursor(mx, my - 10);
    display.print("Color Mosaic:");
    for (int py = 0; py < 6; py++)
      for (int px = 0; px < 6; px++) {
        uint16_t c = rColors[(px + py) % 5];
        display.fillRect(mx + px * 55, my + 10 + py * 55, 50, 50, c);
      }

    // Concentric colored circles (bottom)
    int cx = 300, cy = 850;
    uint16_t ccColors[] = {C_BLUE, C_GREEN, C_YELLOW, C_RED, C_BLACK};
    for (int i = 0; i < 5; i++)
      for (int r = 0; r < 5; r++)
        display.drawCircle(cx, cy, 70 - i * 12 + r, ccColors[i]);

    // Fan of colored lines
    int fx = 800, fy = 900;
    for (int a = 0; a < 180; a += 8) {
      float rad = a * 3.14159f / 180.0f;
      uint16_t lc = rColors[(a / 8) % 5];
      display.drawLine(fx, fy, fx + (int)(100 * cosf(rad)), fy - (int)(100 * sinf(rad)), lc);
    }

    // Large rounded rects (bottom section)
    int rrY = 1050;
    display.fillRoundRect(60, rrY, 250, 150, 15, C_RED);
    display.fillRoundRect(340, rrY, 250, 150, 15, C_GREEN);
    display.fillRoundRect(620, rrY, 250, 150, 15, C_BLUE);
    display.fillRoundRect(900, rrY, 250, 150, 15, C_YELLOW);

    display.setTextColor(C_WHITE);
    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(90, rrY + 90); display.print("Red");
    display.setCursor(370, rrY + 90); display.print("Green");
    display.setCursor(660, rrY + 90); display.print("Blue");
    display.setTextColor(C_BLACK);
    display.setCursor(910, rrY + 90); display.print("Yellow");

    display.setTextColor(C_BLACK);
    drawCenteredText("GFX primitives on 13.3\" e-Paper", H - 30, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 5: Patterns
// =====================================================================
void showColorPatterns()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(C_WHITE);
    display.fillRect(0, 0, W, 50, C_RED);
    display.setTextColor(C_WHITE);
    drawCenteredText("Color Patterns", 38, &FreeSansBold18pt7b);
    display.setTextColor(C_BLACK);
    display.setFont(&FreeSansBold12pt7b);

    uint16_t pColors[] = {C_RED, C_GREEN, C_BLUE, C_YELLOW, C_BLACK};
    int bx = 40, by = 75, bw = 230, bh = 230, gap = 30;

    // Color checkerboard
    display.setCursor(bx + 20, by + 20);
    display.print("Color Checker");
    int tby2 = by + 30;
    for (int py = 0; py < bh / 20; py++)
      for (int px = 0; px < bw / 20; px++)
        display.fillRect(bx + px * 20, tby2 + py * 20, 20, 20, pColors[(px + py) % 5]);

    // Horizontal stripes
    int bx2 = bx + bw + gap;
    display.setCursor(bx2 + 20, by + 20);
    display.print("H-Stripes");
    int tby3 = by + 30;
    for (int py = 0; py < bh; py += 20) {
      int ci = (py / 20) % 5;
      display.fillRect(bx2, tby3 + py, bw, 20, pColors[ci]);
    }

    // Vertical stripes
    int bx3 = bx2 + bw + gap;
    display.setCursor(bx3 + 20, by + 20);
    display.print("V-Stripes");
    int tby4 = by + 30;
    for (int px = 0; px < bw; px += 20) {
      int ci = (px / 20) % 5;
      display.fillRect(bx3 + px, tby4, 20, bh, pColors[ci]);
    }

    // Color dot grid
    int bx4 = bx3 + bw + gap;
    display.setCursor(bx4 + 20, by + 20);
    display.print("Color Dots");
    int tby5 = by + 30;
    for (int py = 0; py < bh; py += 24)
      for (int px = 0; px < bw; px += 24) {
        int ci = (px / 24 + py / 24) % 5;
        display.fillCircle(bx4 + px + 12, tby5 + py + 12, 8, pColors[ci]);
      }

    // Full-width rainbow bars
    int barY = tby5 + bh + 30;
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(bx, barY - 10);
    display.print("Rainbow bars:");
    for (int i = 0; i < 5; i++)
      display.fillRect(bx, barY + 10 + i * 30, W - 2 * bx, 24, pColors[i]);

    // Diamond pattern
    int diaY = barY + 210;
    display.setCursor(bx, diaY - 10);
    display.print("Diamond pattern:");
    for (int py = 0; py < 5; py++)
      for (int px = 0; px < 10; px++) {
        int cx2 = bx + 50 + px * 100;
        int cy2 = diaY + 20 + py * 60;
        uint16_t dc = pColors[(px + py) % 5];
        display.fillTriangle(cx2, cy2, cx2 - 30, cy2 + 30, cx2 + 30, cy2 + 30, dc);
        display.fillTriangle(cx2, cy2 + 60, cx2 - 30, cy2 + 30, cx2 + 30, cy2 + 30, dc);
      }

    drawCenteredText("Patterns with 6-color palette", H - 30, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 6: Dashboard
// =====================================================================
void drawColorCard(int x, int y, int w, int h, const char* title,
                   const char* value, const char* unit, uint16_t accent)
{
  display.drawRoundRect(x, y, w, h, 8, C_BLACK);
  display.fillRoundRect(x + 2, y + 2, w - 4, 34, 6, accent);
  display.setTextColor(C_WHITE);
  display.setFont(&FreeSansBold12pt7b);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(x + (w - tbw) / 2 - tbx, y + 28);
  display.print(title);

  display.setTextColor(accent);
  display.setFont(&FreeSansBold24pt7b);
  display.getTextBounds(value, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(x + (w - tbw) / 2 - tbx, y + h / 2 + 15);
  display.print(value);

  display.setTextColor(C_BLACK);
  display.setFont(&FreeSans9pt7b);
  display.getTextBounds(unit, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(x + (w - tbw) / 2 - tbx, y + h - 15);
  display.print(unit);
}

void showDashboard()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(C_WHITE);
    display.fillRect(0, 0, W, 50, C_BLACK);
    display.setTextColor(C_WHITE);
    drawCenteredText("Dashboard", 38, &FreeSansBold18pt7b);
    display.setTextColor(C_BLACK);

    int cw = 250, ch = 180, gap = 25;
    int sx = (W - 4 * cw - 3 * gap) / 2;
    int row1Y = 75;

    char uptBuf[16]; snprintf(uptBuf, sizeof(uptBuf), "%lu", millis() / 1000);
    char heapBuf[16]; snprintf(heapBuf, sizeof(heapBuf), "%lu", (unsigned long)(ESP.getFreeHeap() / 1024));

    drawColorCard(sx,                    row1Y, cw, ch, "Temp",     "23.5",  "Celsius",  C_RED);
    drawColorCard(sx + cw + gap,         row1Y, cw, ch, "Humidity", "65",    "% RH",     C_BLUE);
    drawColorCard(sx + 2 * (cw + gap),   row1Y, cw, ch, "Heap",    heapBuf, "kB free",  C_GREEN);
    drawColorCard(sx + 3 * (cw + gap),   row1Y, cw, ch, "Uptime",  uptBuf,  "seconds",  C_YELLOW);

    // Activity log
    int logY = row1Y + ch + 30;
    display.drawRoundRect(sx, logY, W - 2 * sx, 300, 8, C_BLACK);
    display.fillRoundRect(sx + 2, logY + 2, W - 2 * sx - 4, 34, 6, C_BLUE);
    display.setTextColor(C_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(sx + 20, logY + 28);
    display.print("Activity Log");

    display.setFont(&FreeMonoBold9pt7b);
    const char* logs[] = {
      " System boot - ESP32-S3 + PSRAM",
      " Panel: T133A01 1200x1600 6-Color",
      " Dual-chip controller (CS + CS1)",
      " SPI @ 4MHz (HSPI)",
      " Framebuffer: 937 KB in PSRAM",
      " Demo: 6 screens completed",
    };
    uint16_t logColors[] = {C_GREEN, C_BLUE, C_YELLOW, C_RED, C_BLACK, C_GREEN};
    int ly = logY + 65;
    for (int i = 0; i < 6; i++) {
      display.fillCircle(sx + 25, ly - 4, 6, logColors[i]);
      display.setTextColor(C_BLACK);
      display.setCursor(sx + 40, ly);
      display.print(logs[i]);
      ly += 36;
    }

    // Multi-color progress bar
    int barY = logY + 310;
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(C_BLACK);
    display.setCursor(sx, barY + 20);
    display.print("Progress:");
    int barX = sx + 200, barW = W - 2 * sx - 260, barH = 30;
    display.drawRect(barX, barY, barW, barH, C_BLACK);
    uint16_t barColors[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK};
    int segW = barW / 5;
    for (int i = 0; i < 5; i++)
      display.fillRect(barX + i * segW, barY + 1, segW, barH - 2, barColors[i]);
    display.setTextColor(C_GREEN);
    display.setFont(&FreeSans9pt7b);
    display.setCursor(barX + barW + 10, barY + 20);
    display.print("100%");

    // Status cards row
    int sY = barY + 60;
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(C_BLACK);
    display.setCursor(sx, sY);
    display.print("System Status:");

    int scW = (W - 2 * sx - 3 * 15) / 4, scH = 100;
    sY += 20;
    uint16_t scColors[] = {C_GREEN, C_BLUE, C_YELLOW, C_RED};
    const char* scLabels[] = {"SPI", "PSRAM", "Display", "Power"};
    const char* scValues[] = {"OK", "8 MB", "Ready", "Stable"};
    for (int i = 0; i < 4; i++) {
      int scX = sx + i * (scW + 15);
      display.fillRoundRect(scX, sY, scW, scH, 8, scColors[i]);
      display.setTextColor(C_WHITE);
      display.setFont(&FreeSansBold18pt7b);
      int16_t tbx2, tby2; uint16_t tbw2, tbh2;
      display.getTextBounds(scValues[i], 0, 0, &tbx2, &tby2, &tbw2, &tbh2);
      display.setCursor(scX + (scW - tbw2) / 2 - tbx2, sY + 45);
      display.print(scValues[i]);
      display.setFont(&FreeSans9pt7b);
      display.getTextBounds(scLabels[i], 0, 0, &tbx2, &tby2, &tbw2, &tbh2);
      display.setCursor(scX + (scW - tbw2) / 2 - tbx2, sY + scH - 12);
      display.print(scLabels[i]);
    }

    display.setTextColor(C_BLACK);
    drawCenteredText("13.3\" 6-color ePaper: vivid and power-efficient", H - 30, &FreeSans9pt7b);
  } while (display.nextPage());
}
