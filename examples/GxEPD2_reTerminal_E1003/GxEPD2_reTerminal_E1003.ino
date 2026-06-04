#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

#include "GxEPD2_ED103TC2_1872x1404.h"

#define DBG Serial0

// ===== reTerminal E1003 pin mapping =====
#define EPD_SCK_PIN       7
#define EPD_MISO_PIN      8
#define EPD_MOSI_PIN      9
#define EPD_CS_PIN        10
#define EPD_RES_PIN       12
#define EPD_BUSY_PIN      13
#define EPD_TFT_ENABLE    11
#define EPD_ITE_ENABLE    21

SPIClass hspi(HSPI);

// ===== Display object =====
#define MAX_DISPLAY_BUFFER_SIZE 65536u
#define MAX_HEIGHT(EPD) \
    (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) \
         ? EPD::HEIGHT \
         : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

GxEPD2_BW<GxEPD2_ED103TC2_1872x1404, MAX_HEIGHT(GxEPD2_ED103TC2_1872x1404)>
    display(GxEPD2_ED103TC2_1872x1404(EPD_CS_PIN, -1, EPD_RES_PIN, EPD_BUSY_PIN));

// ===== Layout constants =====
static const int HEADER_H = 96;
static const int FOOTER_H = 72;
static const int PAGE_MARGIN = 48;
static const int PANEL_GAP = 24;

// ===== Forward declarations =====
void showSplashScreen();
void showSystemInfo();
void showGeometryDemo();
void showPatternDemo();
void showTypographyDemo();
void showDashboardDemo();

void drawCenteredText(const char* text, int16_t y, const GFXfont* font);
void drawCenteredTextInBox(const char* text, int16_t x, int16_t y, int16_t w, int16_t h, const GFXfont* font);
void drawPageHeader(const char* title, const char* subtitle);
void drawPageFooter(const char* text);
void drawPanelFrame(int x, int y, int w, int h);
void drawCardShell(int x, int y, int w, int h, const char* title);
void drawMetricCard(int x, int y, int w, int h, const char* title, const char* value, const char* unit);
void drawInfoCard(int x, int y, int w, int h, const char* label, const char* value, const GFXfont* valueFont);
void drawTypographyCard(int x, int y, int w, int h, const char* title, const char* sample, const GFXfont* font);

// ===== Setup =====
void setup()
{
  DBG.begin(115200);
  delay(100);
  DBG.println();
  DBG.println(F("=================================================="));
  DBG.println(F("[E1003] GxEPD2 reTerminal E1003 Demo"));
  DBG.print  (F("[E1003] PSRAM = ")); DBG.print(ESP.getPsramSize() / 1024);
  DBG.println(F(" kB"));
  DBG.println(F("=================================================="));

  pinMode(EPD_TFT_ENABLE, OUTPUT); digitalWrite(EPD_TFT_ENABLE, HIGH);
  pinMode(EPD_ITE_ENABLE, OUTPUT); digitalWrite(EPD_ITE_ENABLE, HIGH);
  delay(50);

  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_CS_PIN,  OUTPUT); digitalWrite(EPD_CS_PIN, HIGH);

  hspi.begin(EPD_SCK_PIN, EPD_MISO_PIN, EPD_MOSI_PIN, -1);
  display.epd2.selectSPI(hspi, SPISettings(10000000, MSBFIRST, SPI_MODE0));

  display.init(0, true, 10, false);
  DBG.println(F("[E1003] display initialized"));

  // --- Run demo screens ---
  DBG.println(F("[E1003] Screen 1: Splash"));
  showSplashScreen();
  delay(5000);

  DBG.println(F("[E1003] Screen 2: System Info"));
  showSystemInfo();
  delay(5000);

  DBG.println(F("[E1003] Screen 3: Typography"));
  showTypographyDemo();
  delay(5000);

  DBG.println(F("[E1003] Screen 4: Geometry"));
  showGeometryDemo();
  delay(5000);

  DBG.println(F("[E1003] Screen 5: Patterns"));
  showPatternDemo();
  delay(5000);

  DBG.println(F("[E1003] Screen 6: Dashboard"));
  showDashboardDemo();

  DBG.println(F("[E1003] Demo complete. Hibernating."));
  delay(2000);
  display.hibernate();
}

void loop() {}

// =====================================================================
// Helpers
// =====================================================================
void drawCenteredText(const char* text, int16_t y, const GFXfont* font)
{
  display.setFont(font);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor((display.width() - tbw) / 2 - tbx, y);
  display.print(text);
}

void drawCenteredTextInBox(const char* text, int16_t x, int16_t y, int16_t w, int16_t h, const GFXfont* font)
{
  if (!text) return;
  display.setFont(font);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  const int16_t cx = x + (w - tbw) / 2 - tbx;
  const int16_t cy = y + (h - tbh) / 2 - tby;
  display.setCursor(cx, cy);
  display.print(text);
}

void drawPageHeader(const char* title, const char* subtitle)
{
  const uint16_t W = display.width();
  display.fillRect(0, 0, W, HEADER_H, GxEPD_BLACK);

  display.setTextColor(GxEPD_WHITE);
  drawCenteredText(title, 40, &FreeSansBold18pt7b);

  if (subtitle && subtitle[0] != '\0') {
    drawCenteredText(subtitle, 78, &FreeSans9pt7b);
  }

  display.setTextColor(GxEPD_BLACK);
}

void drawPageFooter(const char* text)
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();

  display.drawFastHLine(64, H - 60, W - 128, GxEPD_BLACK);
  display.setFont(&FreeSans9pt7b);
  drawCenteredText(text, H - 34, &FreeSans9pt7b);
}

void drawPanelFrame(int x, int y, int w, int h)
{
  display.drawRoundRect(x, y, w, h, 16, GxEPD_BLACK);
  display.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 14, GxEPD_BLACK);
}

void drawCardShell(int x, int y, int w, int h, const char* title)
{
  drawPanelFrame(x, y, w, h);
  display.fillRoundRect(x + 4, y + 4, w - 8, 38, 10, GxEPD_BLACK);
  display.setTextColor(GxEPD_WHITE);
  drawCenteredTextInBox(title, x + 10, y + 8, w - 20, 24, &FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
}

void drawMetricCard(int x, int y, int w, int h, const char* title, const char* value, const char* unit)
{
  drawCardShell(x, y, w, h, title);
  drawCenteredTextInBox(value, x + 18, y + 58, w - 36, h - 108, &FreeSansBold24pt7b);

  if (unit && unit[0] != '\0') {
    display.setFont(&FreeSans9pt7b);
    drawCenteredTextInBox(unit, x + 20, y + h - 36, w - 40, 20, &FreeSans9pt7b);
  }
}

void drawInfoCard(int x, int y, int w, int h, const char* label, const char* value, const GFXfont* valueFont)
{
  drawCardShell(x, y, w, h, label);
  display.setTextColor(GxEPD_BLACK);
  drawCenteredTextInBox(value, x + 20, y + 54, w - 40, h - 78, valueFont);
}

void drawTypographyCard(int x, int y, int w, int h, const char* title, const char* sample, const GFXfont* font)
{
  drawCardShell(x, y, w, h, title);
  drawCenteredTextInBox(sample, x + 20, y + 54, w - 40, h - 78, font);
}

// =====================================================================
// Screen 1: Splash screen
// =====================================================================
void showSplashScreen()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();

  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Outer frame
    display.drawRect(24, 24, W - 48, H - 48, GxEPD_BLACK);
    display.drawRect(34, 34, W - 68, H - 68, GxEPD_BLACK);

    // Hero panel in the center
    const int heroX = 140;
    const int heroY = 150;
    const int heroW = W - 280;
    const int heroH = H - 420;

    drawPanelFrame(heroX, heroY, heroW, heroH);

    display.fillRoundRect(heroX + 6, heroY + 6, heroW - 12, 54, 12, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredTextInBox("reTerminal E1003", heroX + 20, heroY + 14, heroW - 40, 30, &FreeSansBold18pt7b);

    display.setTextColor(GxEPD_BLACK);
    drawCenteredTextInBox("10.3\" e-Paper Display", heroX + 40, heroY + 96, heroW - 80, 90, &FreeSansBold24pt7b);
    drawCenteredTextInBox("GxEPD2 + IT8951 Driver Demo", heroX + 40, heroY + 210, heroW - 80, 64, &FreeSansBold18pt7b);

    display.drawFastHLine(heroX + 120, heroY + 292, heroW - 240, GxEPD_BLACK);
    drawCenteredTextInBox("1872 x 1404 pixels | ED103TC2 panel", heroX + 40, heroY + 316, heroW - 80, 44, &FreeSansBold12pt7b);
    drawCenteredTextInBox("Powered by ESP32-S3 + PSRAM", heroX + 40, heroY + 366, heroW - 80, 34, &FreeSans9pt7b);

    // Bottom three badges
    const int badgeY = H - 180;
    const int badgeW = (W - 2 * PAGE_MARGIN - 2 * PANEL_GAP) / 3;
    const int badgeH = 92;
    const int badgeX1 = PAGE_MARGIN;
    const int badgeX2 = badgeX1 + badgeW + PANEL_GAP;
    const int badgeX3 = badgeX2 + badgeW + PANEL_GAP;

    drawPanelFrame(badgeX1, badgeY, badgeW, badgeH);
    drawPanelFrame(badgeX2, badgeY, badgeW, badgeH);
    drawPanelFrame(badgeX3, badgeY, badgeW, badgeH);

    drawCenteredTextInBox("Large Canvas", badgeX1 + 12, badgeY + 12, badgeW - 24, badgeH - 24, &FreeSansBold12pt7b);
    drawCenteredTextInBox("High Contrast", badgeX2 + 12, badgeY + 12, badgeW - 24, badgeH - 24, &FreeSansBold12pt7b);
    drawCenteredTextInBox("Low Power Image Hold", badgeX3 + 12, badgeY + 12, badgeW - 24, badgeH - 24, &FreeSansBold12pt7b);

    // Footer note
    drawCenteredText("Seeed Studio x GxEPD2", H - 70, &FreeSans9pt7b);

  } while (display.nextPage());
}

// =====================================================================
// Screen 2: System information
// =====================================================================
void showSystemInfo()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();

  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawPageHeader("System Information", "Hardware, memory and display details laid out as cards");

    const int contentX = PAGE_MARGIN;
    const int contentY = HEADER_H + 18;
    const int contentW = W - 2 * PAGE_MARGIN;
    const int contentH = H - HEADER_H - FOOTER_H - 36;
    const int gapX = PANEL_GAP;
    const int gapY = 16;
    const int cardW = (contentW - gapX) / 2;
    const int cardH = (contentH - 4 * gapY) / 5;

    char psramBuf[32], heapBuf[32], chipBuf[64], dispBuf[32], uptimeBuf[32];
    snprintf(psramBuf, sizeof(psramBuf), "%lu kB", (unsigned long)(ESP.getPsramSize() / 1024));
    snprintf(heapBuf,  sizeof(heapBuf),  "%lu kB free", (unsigned long)(ESP.getFreeHeap() / 1024));
    snprintf(chipBuf,  sizeof(chipBuf),  "ESP32-S3 @ %lu MHz", (unsigned long)(ESP.getCpuFreqMHz()));
    snprintf(dispBuf,  sizeof(dispBuf),  "%d x %d", display.width(), display.height());
    unsigned long sec = millis() / 1000;
    snprintf(uptimeBuf, sizeof(uptimeBuf), "%lu.%01lu s", sec, (millis() % 1000) / 100);

    struct InfoCardSpec {
      const char* label;
      const char* value;
      const GFXfont* font;
    };

    InfoCardSpec cards[] = {
      {"MCU",        chipBuf,     &FreeSansBold18pt7b},
      {"PSRAM",      psramBuf,    &FreeSansBold18pt7b},
      {"Heap",       heapBuf,     &FreeSansBold18pt7b},
      {"Display",    dispBuf,     &FreeSansBold18pt7b},
      {"Panel",      "E Ink ED103TC2", &FreeSansBold12pt7b},
      {"Controller", "IT8951",    &FreeSansBold18pt7b},
      {"Interface",  "SPI (HSPI/SPI3) @ 10MHz", &FreeSansBold12pt7b},
      {"Color",      "Monochrome (BW)", &FreeSansBold18pt7b},
      {"Library",    "GxEPD2",    &FreeSansBold18pt7b},
      {"Uptime",     uptimeBuf,   &FreeSansBold18pt7b},
    };

    for (int i = 0; i < 10; i++) {
      const int col = i % 2;
      const int row = i / 2;
      const int x = contentX + col * (cardW + gapX);
      const int y = contentY + row * (cardH + gapY);
      drawInfoCard(x, y, cardW, cardH, cards[i].label, cards[i].value, cards[i].font);
    }

    drawPageFooter("reTerminal E1003 | GxEPD2 Demo");

  } while (display.nextPage());
}

// =====================================================================
// Screen 3: Typography showcase
// =====================================================================
void showTypographyDemo()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();

  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawPageHeader("Typography Demo", "Font families and sizes shown in a balanced card grid");

    const int contentX = PAGE_MARGIN;
    const int contentY = HEADER_H + 18;
    const int contentW = W - 2 * PAGE_MARGIN;
    const int contentH = H - HEADER_H - FOOTER_H - 36;
    const int gapX = PANEL_GAP;
    const int gapY = PANEL_GAP;
    const int cardW = (contentW - gapX) / 2;
    const int cardH = (contentH - 2 * gapY) / 3;

    drawTypographyCard(contentX,                    contentY,                    cardW, cardH, "FreeSans Bold 24pt", "Display Title", &FreeSansBold24pt7b);
    drawTypographyCard(contentX + cardW + gapX,     contentY,                    cardW, cardH, "FreeSans Bold 18pt", "Section Heading", &FreeSansBold18pt7b);
    drawTypographyCard(contentX,                    contentY + cardH + gapY,     cardW, cardH, "FreeSans Bold 12pt", "Clean and readable", &FreeSansBold12pt7b);
    drawTypographyCard(contentX + cardW + gapX,     contentY + cardH + gapY,     cardW, cardH, "FreeSans 9pt", "Body text for dense data", &FreeSans9pt7b);
    drawTypographyCard(contentX,                    contentY + 2 * (cardH + gapY), cardW, cardH, "FreeMono Bold 12pt", "0123456789 ABCDEF", &FreeMonoBold12pt7b);
    drawTypographyCard(contentX + cardW + gapX,     contentY + 2 * (cardH + gapY), cardW, cardH, "FreeMono 9pt", "Code / tables / logs", &FreeMono9pt7b);

    drawPageFooter("Typography is easier to scan when the page is divided into readable blocks");

  } while (display.nextPage());
}

// =====================================================================
// Screen 4: Geometric shapes
// =====================================================================
void showGeometryDemo()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();

  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawPageHeader("Geometry & Shapes Demo", "Each panel gets enough space to breathe");

    const int contentX = PAGE_MARGIN;
    const int contentY = HEADER_H + 18;
    const int contentW = W - 2 * PAGE_MARGIN;
    const int contentH = H - HEADER_H - FOOTER_H - 36;
    const int gapX = PANEL_GAP;
    const int gapY = PANEL_GAP;
    const int panelW = (contentW - gapX) / 2;
    const int panelH = (contentH - gapY) / 2;

    // Top-left: rectangles
    const int p1x = contentX;
    const int p1y = contentY;
    drawCardShell(p1x, p1y, panelW, panelH, "Rectangles");
    for (int i = 0; i < 4; i++) {
      display.drawRect(p1x + 80 + i * 130, p1y + 100 + i * 10, 100, 70, GxEPD_BLACK);
    }
    display.fillRect(p1x + 650, p1y + 100, 120, 90, GxEPD_BLACK);
    display.drawRect(p1x + 620, p1y + 70, 160, 150, GxEPD_BLACK);

    // Top-right: circles
    const int p2x = contentX + panelW + gapX;
    const int p2y = contentY;
    drawCardShell(p2x, p2y, panelW, panelH, "Circles");
    for (int i = 0; i < 5; i++) {
      display.drawCircle(p2x + 130 + i * 120, p2y + 150, 28 + i * 7, GxEPD_BLACK);
    }
    for (int i = 0; i < 4; i++) {
      display.fillCircle(p2x + 170 + i * 150, p2y + 340, 18 + i * 6, GxEPD_BLACK);
    }

    // Bottom-left: triangles and line fan
    const int p3x = contentX;
    const int p3y = contentY + panelH + gapY;
    drawCardShell(p3x, p3y, panelW, panelH, "Triangles and Lines");
    for (int i = 0; i < 5; i++) {
      int tx = p3x + 90 + i * 120;
      int ty = p3y + 120;
      int sz = 70 + i * 10;
      display.drawTriangle(tx, ty + sz, tx + sz / 2, ty, tx + sz, ty + sz, GxEPD_BLACK);
    }
    const int fanCx = p3x + 180;
    const int fanCy = p3y + 380;
    for (int angle = 0; angle < 180; angle += 12) {
      const float rad = angle * 3.1415926f / 180.0f;
      const int ex = fanCx + (int)(150.0f * cosf(rad));
      const int ey = fanCy - (int)(150.0f * sinf(rad));
      display.drawLine(fanCx, fanCy, ex, ey, GxEPD_BLACK);
    }

    // Bottom-right: concentric circles and rounded rectangles
    const int p4x = contentX + panelW + gapX;
    const int p4y = contentY + panelH + gapY;
    drawCardShell(p4x, p4y, panelW, panelH, "Rounded Shapes");
    const int ccx = p4x + 240;
    const int ccy = p4y + 235;
    for (int r = 30; r <= 150; r += 24) {
      display.drawCircle(ccx, ccy, r, GxEPD_BLACK);
    }
    const int rrx = p4x + 520;
    const int rry = p4y + 120;
    for (int i = 0; i < 4; i++) {
      display.drawRoundRect(rrx + i * 14, rry + i * 14,
                            250 - i * 28, 230 - i * 28,
                            24 - i * 3, GxEPD_BLACK);
    }

    drawPageFooter("Adafruit GFX primitives on a wide e-paper canvas");

  } while (display.nextPage());
}

// =====================================================================
// Screen 5: Patterns
// =====================================================================
void showPatternDemo()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();

  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawPageHeader("Pattern & Fill Demo", "Six pattern tiles arranged in a clean 3 x 2 grid");

    const int contentX = PAGE_MARGIN;
    const int contentY = HEADER_H + 18;
    const int contentW = W - 2 * PAGE_MARGIN;
    const int contentH = H - HEADER_H - FOOTER_H - 36;
    const int gapX = PANEL_GAP;
    const int gapY = PANEL_GAP;
    const int tileW = (contentW - 2 * gapX) / 3;
    const int tileH = (contentH - gapY) / 2;

    struct TileSpec {
      const char* title;
    };

    TileSpec tiles[] = {
      {"Checkerboard"},
      {"H-Stripes"},
      {"V-Stripes"},
      {"Dot Grid"},
      {"Diagonal"},
      {"Dither Gradient"},
    };

    for (int i = 0; i < 6; i++) {
      const int col = i % 3;
      const int row = i / 3;
      const int x = contentX + col * (tileW + gapX);
      const int y = contentY + row * (tileH + gapY);
      drawCardShell(x, y, tileW, tileH, tiles[i].title);

      const int innerX = x + 16;
      const int innerY = y + 58;
      const int innerW = tileW - 32;
      const int innerH = tileH - 74;

      if (i == 0) {
        const int sz = 22;
        for (int py = 0; py < innerH / sz + 1; py++) {
          for (int px = 0; px < innerW / sz + 1; px++) {
            if ((px + py) & 1) {
              display.fillRect(innerX + px * sz, innerY + py * sz, sz, sz, GxEPD_BLACK);
            }
          }
        }
      } else if (i == 1) {
        for (int py = 0; py < innerH; py += 14) {
          if ((py / 14) & 1) {
            display.fillRect(innerX, innerY + py, innerW, 14, GxEPD_BLACK);
          }
        }
      } else if (i == 2) {
        for (int px = 0; px < innerW; px += 14) {
          if ((px / 14) & 1) {
            display.fillRect(innerX + px, innerY, 14, innerH, GxEPD_BLACK);
          }
        }
      } else if (i == 3) {
        for (int py = 0; py < innerH; py += 22) {
          for (int px = 0; px < innerW; px += 22) {
            display.fillCircle(innerX + px + 11, innerY + py + 11, 4, GxEPD_BLACK);
          }
        }
      } else if (i == 4) {
        display.drawRect(innerX, innerY, innerW, innerH, GxEPD_BLACK);
        for (int d = -innerH; d < innerW; d += 18) {
          int x0 = innerX + max(0, d);
          int y0 = innerY + max(0, -d);
          int x1 = innerX + min(innerW - 1, d + innerH - 1);
          int y1 = innerY + min(innerH - 1, -d + innerW - 1);
          display.drawLine(x0, y0, x1, y1, GxEPD_BLACK);
        }
      } else {
        display.drawRect(innerX, innerY, innerW, innerH, GxEPD_BLACK);
        for (int py = 0; py < innerH; py++) {
          for (int px = 0; px < innerW; px++) {
            int density = (px * 255) / innerW;
            if ((((px * 7 + py * 13) ^ (px * py)) & 0xFF) < density) {
              display.drawPixel(innerX + px, innerY + py, GxEPD_BLACK);
            }
          }
        }
      }
    }

    drawPageFooter("Patterns are easier to compare when each sample gets the same amount of space");

  } while (display.nextPage());
}

// =====================================================================
// Screen 6: Dashboard-style layout
// =====================================================================
void showDashboardDemo()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();

  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawPageHeader("Dashboard Demo", "A large, readable layout with metrics, logs and a status bar");

    const int contentX = PAGE_MARGIN;
    const int contentY = HEADER_H + 18;
    const int contentW = W - 2 * PAGE_MARGIN;
    const int contentH = H - HEADER_H - FOOTER_H - 36;
    const int gapX = PANEL_GAP;
    const int gapY = PANEL_GAP;

    // Top row metrics
    const int metricH = 220;
    const int metricW = (contentW - 3 * gapX) / 4;

    char uptimeBuf[16];
    snprintf(uptimeBuf, sizeof(uptimeBuf), "%lu", millis() / 1000);

    char heapBuf[16];
    snprintf(heapBuf, sizeof(heapBuf), "%lu", (unsigned long)(ESP.getFreeHeap() / 1024));

    drawMetricCard(contentX + 0 * (metricW + gapX), contentY, metricW, metricH, "Temperature", "23.5", "Celsius");
    drawMetricCard(contentX + 1 * (metricW + gapX), contentY, metricW, metricH, "Humidity", "65", "% RH");
    drawMetricCard(contentX + 2 * (metricW + gapX), contentY, metricW, metricH, "Free Heap", heapBuf, "kB");
    drawMetricCard(contentX + 3 * (metricW + gapX), contentY, metricW, metricH, "Uptime", uptimeBuf, "seconds");

    // Activity log panel
    const int logY = contentY + metricH + gapY;
    const int logH = 460;
    drawCardShell(contentX, logY, contentW, logH, "Activity Log");

    const char* logEntries[] = {
      "[00:00:01] System boot complete - ESP32-S3 @ 240MHz",
      "[00:00:02] PSRAM initialized: 8192 kB available",
      "[00:00:03] IT8951 controller detected (FW: eSee_d.v.0)",
      "[00:00:03] Panel: ED103TC2 1872x1404, VCOM=-1.40V",
      "[00:00:04] SPI bus configured: HSPI @ 10MHz",
      "[00:00:05] Display driver ready - full refresh mode",
      "[00:00:06] Demo sequence started...",
    };

    display.setFont(&FreeMono9pt7b);
    const int lineX = contentX + 24;
    const int lineY = logY + 72;
    const int lineH = 48;

    for (int i = 0; i < 7; i++) {
      display.setCursor(lineX, lineY + i * lineH);
      display.print(logEntries[i]);
      if (i < 6) {
        display.drawFastHLine(contentX + 20, lineY + i * lineH + 18, contentW - 40, GxEPD_BLACK);
      }
    }

    // Bottom status strip
    const int statusY = logY + logH + gapY;
    const int statusH = contentH - metricH - logH - 2 * gapY;
    drawPanelFrame(contentX, statusY, contentW, statusH);

    display.fillRoundRect(contentX + 4, statusY + 4, contentW - 8, 42, 10, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredTextInBox("Progress", contentX + 20, statusY + 10, 160, 24, &FreeSansBold12pt7b);

    const int barX = contentX + 180;
    const int barY = statusY + 18;
    const int barW = contentW - 360;
    const int barH = 28;

    display.setTextColor(GxEPD_BLACK);
    display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
    display.fillRect(barX + 2, barY + 2, barW - 4, barH - 4, GxEPD_BLACK);

    display.setFont(&FreeSans9pt7b);
    display.setCursor(barX + barW + 16, barY + 22);
    display.print("100% - Demo Complete!");

    display.setFont(&FreeSans9pt7b);
    display.setCursor(contentX + 24, statusY + statusH - 18);
    display.print("E-paper is ideal for dashboards: the image stays visible without refresh power");

    drawPageFooter("Wide screens work better when the key information is grouped into strong blocks");

  } while (display.nextPage());
}
