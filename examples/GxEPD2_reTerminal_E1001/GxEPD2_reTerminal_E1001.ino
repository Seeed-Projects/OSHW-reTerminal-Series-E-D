#include <SPI.h>
#include <GxEPD2_BW.h>
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

// ===== Display: 7.5" B&W 800x480 =====
#define MAX_DISPLAY_BUFFER_SIZE 16000u
#define MAX_HEIGHT(EPD) \
    (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) \
         ? EPD::HEIGHT \
         : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

GxEPD2_BW<GxEPD2_750_GDEY075T7, MAX_HEIGHT(GxEPD2_750_GDEY075T7)>
    display(GxEPD2_750_GDEY075T7(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println(F("[E1001] GxEPD2 reTerminal E1001 Demo (7.5\" B&W)"));

  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_DC_PIN,  OUTPUT);
  pinMode(EPD_CS_PIN,  OUTPUT);

  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);
  display.epd2.selectSPI(hspi, SPISettings(2000000, MSBFIRST, SPI_MODE0));
  display.init(0);

  Serial.println(F("[E1001] Screen 1: Splash"));
  showSplashScreen();
  delay(3000);

  Serial.println(F("[E1001] Screen 2: System Info"));
  showSystemInfo();
  delay(3000);

  Serial.println(F("[E1001] Screen 3: Typography"));
  showTypographyDemo();
  delay(3000);

  Serial.println(F("[E1001] Screen 4: Geometry"));
  showGeometryDemo();
  delay(3000);

  Serial.println(F("[E1001] Screen 5: Patterns"));
  showPatternDemo();
  delay(3000);

  Serial.println(F("[E1001] Screen 6: Dashboard"));
  showDashboardDemo();

  Serial.println(F("[E1001] Demo complete. Hibernating."));
  delay(2000);
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
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(10, 10, W - 20, H - 20, GxEPD_BLACK);
    display.drawRect(14, 14, W - 28, H - 28, GxEPD_BLACK);

    display.setTextColor(GxEPD_BLACK);
    drawCenteredText("reTerminal E1001", H / 2 - 60, &FreeSansBold24pt7b);
    drawCenteredText("7.5\" e-Paper Display", H / 2 - 10, &FreeSansBold12pt7b);
    display.drawFastHLine(W / 4, H / 2 + 10, W / 2, GxEPD_BLACK);
    drawCenteredText("GxEPD2 + UC8179 Driver Demo", H / 2 + 45, &FreeSansBold12pt7b);
    drawCenteredText("800 x 480 pixels | Black & White", H / 2 + 75, &FreeSans9pt7b);
    drawCenteredText("Seeed Studio x GxEPD2", H - 40, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 2: System Info
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
    display.fillRect(0, 0, W, 40, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredText("System Information", 30, &FreeSansBold12pt7b);

    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    const char* labels[] = {"MCU", "Display", "Panel", "Controller", "Interface", "Color Depth"};
    char chipBuf[48];
    snprintf(chipBuf, sizeof(chipBuf), "ESP32-S3 @ %lu MHz", (unsigned long)ESP.getCpuFreqMHz());
    const char* values[] = {chipBuf, "800 x 480", "GDEY075T7", "UC8179", "SPI (HSPI) @ 2MHz", "B&W (1-bit)"};

    int y = 95;
    for (int i = 0; i < 6; i++) {
      display.setCursor(50, y);
      display.print(labels[i]);
      display.setCursor(250, y);
      display.print(": ");
      display.print(values[i]);
      y += 48;
      if (i < 5) display.drawFastHLine(50, y - 18, W - 100, GxEPD_BLACK);
    }

    display.drawFastHLine(30, H - 40, W - 60, GxEPD_BLACK);
    drawCenteredText("reTerminal E1001 | GxEPD2 Demo", H - 20, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 3: Typography
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
    display.fillRect(0, 0, W, 40, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredText("Typography Demo", 30, &FreeSansBold12pt7b);

    display.setTextColor(GxEPD_BLACK);
    int y = 85, x = 40;

    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(x, y); display.print("Sans Bold 24pt");
    y += 65;

    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(x, y); display.print("Sans Bold 18pt");
    y += 50;

    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(x, y); display.print("Sans Bold 12pt - Clean and Modern");
    y += 42;

    display.setFont(&FreeSans9pt7b);
    display.setCursor(x, y); display.print("Sans 9pt - Body text for dense info display.");
    y += 40;

    display.drawFastHLine(x, y, W - 80, GxEPD_BLACK);
    y += 25;

    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(x, y); display.print("Mono Bold 12pt");
    y += 38;

    display.setFont(&FreeMono9pt7b);
    display.setCursor(x, y); display.print("Mono 9pt: 0123456789 ABCDEF");
    y += 38;

    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(x, y); display.print("0 1 2 3 4 5 6 7 8 9");

    drawCenteredText("Multiple fonts supported", H - 20, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 4: Geometry
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
    display.fillRect(0, 0, W, 40, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredText("Geometry Demo", 30, &FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);

    // Rectangles
    for (int i = 0; i < 4; i++)
      display.drawRect(40 + i * 80, 60, 60, 50, GxEPD_BLACK);
    display.fillRect(40 + 4 * 80, 60, 60, 50, GxEPD_BLACK);

    // Circles
    for (int i = 0; i < 4; i++)
      display.drawCircle(70 + i * 90, 170, 25 + i * 3, GxEPD_BLACK);
    for (int i = 0; i < 3; i++)
      display.fillCircle(70 + (i + 4) * 90, 170, 25 + i * 3, GxEPD_BLACK);

    // Triangles
    for (int i = 0; i < 5; i++) {
      int tx = 50 + i * 120, sz = 40 + i * 5;
      display.drawTriangle(tx, 270 + sz, tx + sz / 2, 270, tx + sz, 270 + sz, GxEPD_BLACK);
    }

    // Fan of lines
    int fcx = 150, fcy = 410;
    for (int a = 0; a < 180; a += 12) {
      float rad = a * 3.14159f / 180.0f;
      display.drawLine(fcx, fcy, fcx + (int)(60 * cosf(rad)), fcy - (int)(60 * sinf(rad)), GxEPD_BLACK);
    }

    // Concentric circles
    for (int r = 8; r <= 56; r += 8)
      display.drawCircle(400, 400, r, GxEPD_BLACK);

    // Rounded rects
    for (int i = 0; i < 3; i++)
      display.drawRoundRect(550 + i * 8, 340 + i * 8, 120 - i * 16, 100 - i * 16, 8 + i * 4, GxEPD_BLACK);

    drawCenteredText("Adafruit GFX primitives on 7.5\" e-Paper", H - 15, &FreeSans9pt7b);
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
    display.fillRect(0, 0, W, 40, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredText("Pattern Demo", 30, &FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSans9pt7b);

    int bx = 30, by = 55, bw = 150, bh = 150, gap = 30;

    // Checkerboard
    display.setCursor(bx + 25, by + 15); display.print("Checker");
    by += 20;
    for (int py = 0; py < bh / 12; py++)
      for (int px = 0; px < bw / 12; px++)
        if ((px + py) & 1)
          display.fillRect(bx + px * 12, by + py * 12, 12, 12, GxEPD_BLACK);

    // H-stripes
    int bx2 = bx + bw + gap; int by2 = by - 20;
    display.setCursor(bx2 + 25, by2 + 15); display.print("H-Stripes");
    by2 += 20;
    for (int py = 0; py < bh; py += 8)
      if ((py / 8) & 1)
        display.fillRect(bx2, by2 + py, bw, 8, GxEPD_BLACK);

    // V-stripes
    int bx3 = bx2 + bw + gap; int by3 = by2 - 20;
    display.setCursor(bx3 + 25, by3 + 15); display.print("V-Stripes");
    by3 += 20;
    for (int px = 0; px < bw; px += 8)
      if ((px / 8) & 1)
        display.fillRect(bx3 + px, by3, 8, bh, GxEPD_BLACK);

    // Dot grid
    int bx4 = bx3 + bw + gap; int by4 = by3 - 20;
    display.setCursor(bx4 + 30, by4 + 15); display.print("Dot Grid");
    by4 += 20;
    for (int py = 0; py < bh; py += 12)
      for (int px = 0; px < bw; px += 12)
        display.fillCircle(bx4 + px + 6, by4 + py + 6, 3, GxEPD_BLACK);

    // Dither gradient (bottom)
    int gx = 30, gy = 310, gw = W - 60, gh = 120;
    display.setFont(&FreeSans9pt7b);
    display.setCursor(gx, gy - 5); display.print("Dither Gradient:");
    display.drawRect(gx, gy, gw, gh, GxEPD_BLACK);
    for (int py = 0; py < gh; py++)
      for (int px = 0; px < gw; px++) {
        int density = (px * 255) / gw;
        if ((((px * 7 + py * 13) ^ (px * py)) & 0xFF) < density)
          display.drawPixel(gx + px, gy + py, GxEPD_BLACK);
      }

    drawCenteredText("Fill patterns and dithering", H - 15, &FreeSans9pt7b);
  } while (display.nextPage());
}

// =====================================================================
// Screen 6: Dashboard
// =====================================================================
void drawCard(int x, int y, int w, int h, const char* title, const char* value, const char* unit)
{
  display.drawRoundRect(x, y, w, h, 6, GxEPD_BLACK);
  display.fillRoundRect(x + 2, y + 2, w - 4, 26, 4, GxEPD_BLACK);
  display.setTextColor(GxEPD_WHITE);
  display.setFont(&FreeSansBold12pt7b);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(x + (w - tbw) / 2 - tbx, y + 22);
  display.print(title);

  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSansBold18pt7b);
  display.getTextBounds(value, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(x + (w - tbw) / 2 - tbx, y + h / 2 + 12);
  display.print(value);

  display.setFont(&FreeSans9pt7b);
  display.getTextBounds(unit, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(x + (w - tbw) / 2 - tbx, y + h - 10);
  display.print(unit);
}

void showDashboardDemo()
{
  const uint16_t W = display.width();
  const uint16_t H = display.height();
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.fillRect(0, 0, W, 40, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredText("Dashboard Demo", 30, &FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);

    int cw = 170, ch = 130, gap = 20;
    int sx = (W - 4 * cw - 3 * gap) / 2;
    int row1Y = 60;

    char uptBuf[16]; snprintf(uptBuf, sizeof(uptBuf), "%lu", millis() / 1000);
    char heapBuf[16]; snprintf(heapBuf, sizeof(heapBuf), "%lu", (unsigned long)(ESP.getFreeHeap() / 1024));

    drawCard(sx,                       row1Y, cw, ch, "Temp",     "23.5",  "Celsius");
    drawCard(sx + cw + gap,            row1Y, cw, ch, "Humidity", "65",    "% RH");
    drawCard(sx + 2 * (cw + gap),      row1Y, cw, ch, "Heap",    heapBuf, "kB free");
    drawCard(sx + 3 * (cw + gap),      row1Y, cw, ch, "Uptime",  uptBuf,  "seconds");

    // Log area
    int logY = row1Y + ch + 20;
    display.drawRoundRect(sx, logY, W - 2 * sx, 200, 6, GxEPD_BLACK);
    display.fillRoundRect(sx + 2, logY + 2, W - 2 * sx - 4, 26, 4, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(sx + 15, logY + 22);
    display.print("Activity Log");
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMono9pt7b);

    const char* logs[] = {
      "[00:01] System boot - ESP32-S3",
      "[00:02] Panel: GDEY075T7 800x480",
      "[00:03] UC8179 controller ready",
      "[00:04] SPI @ 2MHz (HSPI)",
      "[00:05] Demo sequence started",
    };
    int ly = logY + 50;
    for (int i = 0; i < 5; i++) {
      display.setCursor(sx + 15, ly);
      display.print(logs[i]);
      ly += 28;
    }

    // Progress bar
    int barY = logY + 210;
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(sx, barY + 15);
    display.print("Progress:");
    int barX = sx + 170, barW = W - 2 * sx - 180, barH = 20;
    display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
    display.fillRect(barX + 2, barY + 2, barW - 4, barH - 4, GxEPD_BLACK);
    display.setFont(&FreeSans9pt7b);
    display.setCursor(barX + barW + 8, barY + 14);
    display.print("100%");

    drawCenteredText("E-paper: zero power to maintain image", H - 15, &FreeSans9pt7b);
  } while (display.nextPage());
}
