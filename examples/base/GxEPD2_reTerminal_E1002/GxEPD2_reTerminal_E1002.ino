#include <SPI.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

// ===== Pin mapping =====
#define EPD_SCK_PIN   7
#define EPD_MOSI_PIN  9
#define EPD_CS_PIN    10
#define EPD_DC_PIN    11
#define EPD_RES_PIN   12
#define EPD_BUSY_PIN  13

SPIClass hspi(HSPI);

// ===== Display: 7.3" 6-Color 800x480 =====
#define MAX_DISPLAY_BUFFER_SIZE 16000u
#define MAX_HEIGHT(EPD) \
    (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) \
         ? EPD::HEIGHT \
         : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))

GxEPD2_7C<GxEPD2_730c_GDEP073E01, MAX_HEIGHT(GxEPD2_730c_GDEP073E01)>
    display(GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));

// Convenience color names for the GDEP073E01 6-color palette.
// Note: GxEPD_ORANGE is intentionally NOT used — this panel has no orange,
// and the driver would silently map it to the closest available color
// (usually yellow or red), producing wrong-looking renders.
#define C_BLACK   GxEPD_BLACK
#define C_WHITE   GxEPD_WHITE
#define C_GREEN   GxEPD_GREEN
#define C_BLUE    GxEPD_BLUE
#define C_RED     GxEPD_RED
#define C_YELLOW  GxEPD_YELLOW

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println(F("[E1002] GxEPD2 reTerminal E1002 Demo (7.3\" 6-Color)"));

  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_DC_PIN,  OUTPUT);
  pinMode(EPD_CS_PIN,  OUTPUT);

  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);
  display.epd2.selectSPI(hspi, SPISettings(2000000, MSBFIRST, SPI_MODE0));
  display.init(0);

  // E1002 is a 6-color panel. Each full refresh itself takes ~25-30s, and
  // we want each rendered page to stay visible for a good while before the
  // next refresh kicks in. Keep PAGE_HOLD_MS >= 60s.
  const uint32_t PAGE_HOLD_MS = 60000;

  Serial.println(F("[E1002] Screen 1: Splash"));
  showSplashScreen();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1002] Screen 2: Color Palette"));
  showColorPalette();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1002] Screen 3: Color Typography"));
  showColorTypography();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1002] Screen 4: Color Geometry"));
  showColorGeometry();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1002] Screen 5: Color Patterns"));
  showColorPatterns();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1002] Screen 6: Dashboard"));
  showDashboard();
  delay(PAGE_HOLD_MS);

  Serial.println(F("[E1002] Demo complete. Hibernating."));
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

    // Colorful top stripe — 6 native colors
    int stripeH = 12, stripeY = 10;
    uint16_t colors[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK, C_RED};
    int stripeW = (W - 20) / 6;
    for (int i = 0; i < 6; i++)
      display.fillRect(10 + i * stripeW, stripeY, stripeW, stripeH, colors[i]);

    display.drawRect(10, 30, W - 20, H - 40, C_BLACK);

    display.setTextColor(C_BLACK);
    drawCenteredText("reTerminal E1002", H / 2 - 60, &FreeSansBold24pt7b);

    display.setTextColor(C_RED);
    drawCenteredText("7.3\" 6-Color e-Paper", H / 2 - 10, &FreeSansBold12pt7b);

    display.drawFastHLine(W / 4, H / 2 + 15, W / 2, C_BLUE);

    display.setTextColor(C_GREEN);
    drawCenteredText("GxEPD2 + GDEP073E01 Demo", H / 2 + 50, &FreeSansBold12pt7b);

    display.setTextColor(C_BLUE);
    drawCenteredText("800 x 480 | 6 Colors", H / 2 + 85, &FreeSans9pt7b);

    // Bottom colorful stripe (reversed order)
    uint16_t bottomColors[] = {C_BLUE, C_GREEN, C_YELLOW, C_RED, C_BLACK, C_BLUE};
    for (int i = 0; i < 6; i++)
      display.fillRect(10 + i * stripeW, H - 22, stripeW, stripeH, bottomColors[i]);

    display.setTextColor(C_BLACK);
    drawCenteredText("Seeed Studio x GxEPD2", H - 35, &FreeSans9pt7b);
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
    display.fillRect(0, 0, W, 40, C_BLACK);
    display.setTextColor(C_WHITE);
    drawCenteredText("6-Color Palette", 30, &FreeSansBold12pt7b);

    const uint16_t swatchColors[] = {C_BLACK, C_WHITE, C_RED, C_GREEN, C_BLUE, C_YELLOW};
    const char* names[] = {"Black", "White", "Red", "Green", "Blue", "Yellow"};
    int sw = 110, sh = 140, gap = 18;
    int sx = (W - 6 * sw - 5 * gap) / 2;
    int sy = 70;

    for (int i = 0; i < 6; i++) {
      int x = sx + i * (sw + gap);
      display.fillRoundRect(x, sy, sw, sh, 6, swatchColors[i]);
      display.drawRoundRect(x, sy, sw, sh, 6, C_BLACK);
      display.setFont(&FreeSansBold12pt7b);
      display.setTextColor(C_BLACK);
      int16_t tbx, tby; uint16_t tbw, tbh;
      display.getTextBounds(names[i], 0, 0, &tbx, &tby, &tbw, &tbh);
      display.setCursor(x + (sw - tbw) / 2 - tbx, sy + sh + 25);
      display.print(names[i]);
    }

    // Mixed-color row: circles on different backgrounds (5 combos)
    int row2Y = sy + sh + 60;
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(C_BLACK);
    display.setCursor(sx, row2Y - 5);
    display.print("Color combinations:");

    int cx = sx + 40;
    uint16_t bgColors[] =  {C_RED,    C_GREEN, C_BLUE,   C_YELLOW, C_BLACK};
    uint16_t fgColors[] =  {C_YELLOW, C_RED,   C_YELLOW, C_BLUE,   C_RED};
    for (int i = 0; i < 5; i++) {
      int x = cx + i * 130;
      display.fillRoundRect(x, row2Y + 10, 80, 80, 8, bgColors[i]);
      display.fillCircle(x + 40, row2Y + 50, 25, fgColors[i]);
    }

    // Bottom row: colored bars as gradient (5 bars, no orange)
    int barY = row2Y + 110;
    display.setTextColor(C_BLACK);
    display.setCursor(sx, barY - 5);
    display.print("Full-width color bars:");
    uint16_t barColors[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK};
    int barH = 18;
    for (int i = 0; i < 5; i++)
      display.fillRect(sx, barY + 10 + i * (barH + 4), W - 2 * sx, barH, barColors[i]);

    drawCenteredText("All 6 native colors on the GDEP073E01 panel", H - 15, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 3: Color Typography
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
    display.fillRect(0, 0, W, 40, C_BLUE);
    display.setTextColor(C_WHITE);
    drawCenteredText("Color Typography", 30, &FreeSansBold12pt7b);

    int y = 82, x = 40;

    display.setTextColor(C_BLACK);
    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(x, y); display.print("Black");
    display.setTextColor(C_RED);
    display.setCursor(x + 200, y); display.print("Red");
    display.setTextColor(C_GREEN);
    display.setCursor(x + 340, y); display.print("Green");
    display.setTextColor(C_BLUE);
    display.setCursor(x + 530, y); display.print("Blue");
    y += 58;

    display.setTextColor(C_YELLOW);
    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(x, y); display.print("Yellow text - warm and bright");
    y += 48;

    display.setTextColor(C_RED);
    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(x, y); display.print("Red - emphasis and warnings");
    y += 52;

    display.drawFastHLine(x, y, W - 80, C_RED);
    y += 22;

    // Inverted: white text on color rectangles (4 combos)
    display.setFont(&FreeSansBold12pt7b);
    int bx = x, bw = 220, bh = 40, bgap = 16;
    uint16_t tColors[] = {C_RED, C_GREEN, C_BLUE, C_BLACK};
    const char* labels[] = {"White on Red", "White on Green", "White on Blue", "White on Black"};
    for (int i = 0; i < 4; i++) {
      int by2 = y + i * (bh + bgap);
      display.fillRoundRect(bx, by2, bw, bh, 4, tColors[i]);
      display.setTextColor(C_WHITE);
      display.setCursor(bx + 10, by2 + 28);
      display.print(labels[i]);
    }

    // Right column: Black bg with all native colors
    int rbx = bx + bw + 50;
    display.fillRoundRect(rbx, y, 310, 218, 6, C_BLACK);
    display.setFont(&FreeSansBold18pt7b);
    int ry = y + 38;
    uint16_t rColors[] = {C_RED, C_GREEN, C_BLUE, C_YELLOW, C_WHITE};
    const char* rLabels[] = {"Red",  "Green", "Blue", "Yellow", "White"};
    for (int i = 0; i < 5; i++) {
      display.setTextColor(rColors[i]);
      display.setCursor(rbx + 20, ry);
      display.print(rLabels[i]);
      ry += 38;
    }

    display.setTextColor(C_BLACK);
    drawCenteredText("6-color text rendering", H - 15, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 4: Color Geometry
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
    display.fillRect(0, 0, W, 40, C_GREEN);
    display.setTextColor(C_WHITE);
    drawCenteredText("Color Geometry", 30, &FreeSansBold12pt7b);
    display.setTextColor(C_BLACK);

    // Colored rectangles cascade (5 colors)
    uint16_t rcColors[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK};
    for (int i = 0; i < 5; i++)
      display.fillRect(40 + i * 55, 60 + i * 14, 110, 65, rcColors[i]);

    // Colored circles (5 colors)
    uint16_t ccColors[] = {C_BLUE, C_RED, C_GREEN, C_YELLOW, C_BLACK};
    for (int i = 0; i < 5; i++)
      display.fillCircle(500 + i * 60, 100, 25, ccColors[i]);

    // Colored triangles (5 colors)
    int ty = 210;
    uint16_t triColors[] = {C_RED, C_GREEN, C_BLUE, C_YELLOW, C_BLACK};
    for (int i = 0; i < 5; i++) {
      int tx2 = 60 + i * 130;
      display.fillTriangle(tx2, ty + 60, tx2 + 30, ty, tx2 + 60, ty + 60, triColors[i]);
    }

    // Olympic rings
    int oly = 340, ox = 200;
    uint16_t olyColors[] = {C_BLUE, C_BLACK, C_RED, C_YELLOW, C_GREEN};
    int olyX[] = {0, 70, 140, 35, 105};
    int olyY[] = {0, 0, 0, 35, 35};
    for (int i = 0; i < 5; i++)
      for (int r = 0; r < 4; r++)
        display.drawCircle(ox + olyX[i], oly + olyY[i], 30 - r, olyColors[i]);

    // 6-color swatch grid (2 rows x 3 cols)
    int wx = 560, wy = 310;
    display.fillRect(wx,        wy,      45, 45, C_RED);
    display.fillRect(wx + 45,   wy,      45, 45, C_YELLOW);
    display.fillRect(wx + 90,   wy,      45, 45, C_GREEN);
    display.fillRect(wx,        wy + 45, 45, 45, C_BLUE);
    display.fillRect(wx + 45,   wy + 45, 45, 45, C_BLACK);
    display.fillRect(wx + 90,   wy + 45, 45, 45, C_WHITE);
    display.drawRect(wx + 90,   wy + 45, 45, 45, C_BLACK);

    // Concentric circles (5 colors)
    uint16_t concColors[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK};
    for (int r = 0; r < 5; r++) {
      for (int t = 0; t < 3; t++)
        display.drawCircle(150, 410, 55 - r * 11 + t, concColors[r]);
    }

    display.setTextColor(C_BLACK);
    drawCenteredText("Colorful GFX primitives", H - 15, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 5: Color Patterns
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
    display.fillRect(0, 0, W, 40, C_RED);
    display.setTextColor(C_WHITE);
    drawCenteredText("Color Patterns", 30, &FreeSansBold12pt7b);
    display.setTextColor(C_BLACK);
    display.setFont(&FreeSans9pt7b);

    int bx = 30, by = 55, bw = 150, bh = 150, gap = 25;
    uint16_t pColors[] = {C_RED, C_GREEN, C_BLUE, C_YELLOW, C_BLACK};
    const int nColors = 5;

    // Colored checkerboard
    display.setCursor(bx + 10, by + 15);
    display.print("Color Check");
    by += 20;
    for (int py = 0; py < bh / 15; py++)
      for (int px = 0; px < bw / 15; px++)
        display.fillRect(bx + px * 15, by + py * 15, 15, 15, pColors[(px + py) % nColors]);

    // Horizontal color stripes
    int bx2 = bx + bw + gap; int by2 = by - 20;
    display.setCursor(bx2 + 10, by2 + 15); display.print("H-Stripes");
    by2 += 20;
    for (int py = 0; py < bh; py += 15) {
      int ci = (py / 15) % nColors;
      display.fillRect(bx2, by2 + py, bw, 15, pColors[ci]);
    }

    // Vertical color stripes
    int bx3 = bx2 + bw + gap; int by3 = by2 - 20;
    display.setCursor(bx3 + 10, by3 + 15); display.print("V-Stripes");
    by3 += 20;
    for (int px = 0; px < bw; px += 15) {
      int ci = (px / 15) % nColors;
      display.fillRect(bx3 + px, by3, 15, bh, pColors[ci]);
    }

    // Colored dot grid
    int bx4 = bx3 + bw + gap; int by4 = by3 - 20;
    display.setCursor(bx4 + 10, by4 + 15); display.print("Color Dots");
    by4 += 20;
    for (int py = 0; py < bh; py += 18)
      for (int px = 0; px < bw; px += 18) {
        int ci = (px / 18 + py / 18) % nColors;
        display.fillCircle(bx4 + px + 9, by4 + py + 9, 6, pColors[ci]);
      }

    // Full-width color bar sequence
    int barY = by4 + bh + 20;
    display.setCursor(bx, barY - 5);
    display.print("Color bar sequence:");
    for (int i = 0; i < nColors; i++)
      display.fillRect(bx, barY + 10 + i * 24, W - 2 * bx, 20, pColors[i]);

    drawCenteredText("Patterns with the 6 native colors", H - 15, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 6: Dashboard
// =====================================================================
void drawColorCard(int x, int y, int w, int h, const char* title,
                   const char* value, const char* unit, uint16_t accent)
{
  display.drawRoundRect(x, y, w, h, 6, C_BLACK);
  display.fillRoundRect(x + 2, y + 2, w - 4, 26, 4, accent);
  display.setTextColor(C_WHITE);
  display.setFont(&FreeSansBold12pt7b);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(x + (w - tbw) / 2 - tbx, y + 22);
  display.print(title);

  display.setTextColor(accent);
  display.setFont(&FreeSansBold18pt7b);
  display.getTextBounds(value, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(x + (w - tbw) / 2 - tbx, y + h / 2 + 12);
  display.print(value);

  display.setTextColor(C_BLACK);
  display.setFont(&FreeSans9pt7b);
  display.getTextBounds(unit, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(x + (w - tbw) / 2 - tbx, y + h - 10);
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
    display.fillRect(0, 0, W, 40, C_BLACK);
    display.setTextColor(C_WHITE);
    drawCenteredText("Dashboard", 30, &FreeSansBold12pt7b);
    display.setTextColor(C_BLACK);

    int cw = 170, ch = 130, gap = 20;
    int sx = (W - 4 * cw - 3 * gap) / 2;
    int row1Y = 60;

    char uptBuf[16]; snprintf(uptBuf, sizeof(uptBuf), "%lu", millis() / 1000);
    char heapBuf[16]; snprintf(heapBuf, sizeof(heapBuf), "%lu", (unsigned long)(ESP.getFreeHeap() / 1024));

    drawColorCard(sx,                    row1Y, cw, ch, "Temp",     "23.5",  "Celsius",  C_RED);
    drawColorCard(sx + cw + gap,         row1Y, cw, ch, "Humidity", "65",    "% RH",     C_BLUE);
    drawColorCard(sx + 2 * (cw + gap),   row1Y, cw, ch, "Heap",    heapBuf, "kB free",  C_GREEN);
    drawColorCard(sx + 3 * (cw + gap),   row1Y, cw, ch, "Uptime",  uptBuf,  "seconds",  C_BLACK);

    // Log area with colored markers
    int logY = row1Y + ch + 20;
    display.drawRoundRect(sx, logY, W - 2 * sx, 200, 6, C_BLACK);
    display.fillRoundRect(sx + 2, logY + 2, W - 2 * sx - 4, 26, 4, C_BLUE);
    display.setTextColor(C_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(sx + 15, logY + 22);
    display.print("Activity Log");
    display.setFont(&FreeMono9pt7b);

    const char* logs[] = {
      " System boot - ESP32-S3",
      " Panel: GDEP073E01 6-Color",
      " ED2208 controller ready",
      " SPI @ 2MHz (HSPI)",
      " Demo: 6 screens completed",
    };
    uint16_t logColors[] = {C_GREEN, C_BLUE, C_YELLOW, C_RED, C_GREEN};
    int ly = logY + 50;
    for (int i = 0; i < 5; i++) {
      display.fillCircle(sx + 20, ly - 4, 5, logColors[i]);
      display.setTextColor(C_BLACK);
      display.setCursor(sx + 30, ly);
      display.print(logs[i]);
      ly += 28;
    }

    // Multi-color progress bar
    int barY = logY + 210;
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(C_BLACK);
    display.setCursor(sx, barY + 15);
    display.print("Progress:");
    int barX = sx + 170, barW = W - 2 * sx - 210, barH = 20;
    display.drawRect(barX, barY, barW, barH, C_BLACK);
    uint16_t barColors[] = {C_RED, C_YELLOW, C_GREEN, C_BLUE, C_BLACK};
    int segW = barW / 5;
    for (int i = 0; i < 5; i++)
      display.fillRect(barX + i * segW, barY + 1, segW, barH - 2, barColors[i]);

    display.setTextColor(C_GREEN);
    display.setFont(&FreeSans9pt7b);
    display.setCursor(barX + barW + 8, barY + 14);
    display.print("100%");

    display.setTextColor(C_BLACK);
    drawCenteredText("6-color ePaper: vivid and power-efficient", H - 15, &FreeSans9pt7b);
  } while (display.nextPage());
}
