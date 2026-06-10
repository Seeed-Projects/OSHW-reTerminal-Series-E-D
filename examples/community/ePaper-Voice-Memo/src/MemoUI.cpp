#include "MemoUI.h"
#include "DateLabels.h"
#include "UiLang.h"

#include <ctype.h>

namespace {

// UI colors picked per panel capability so the same drawing code reuses each
// panel's actual headroom: E1001 gray4, E1002 six-color, E1003 gray16.
#if VM_SCREEN_MODE == VM_SCREEN_GRAY4
constexpr uint16_t kUiBg       = TFT_GRAY_3;   // page background
constexpr uint16_t kUiCard     = TFT_GRAY_2;   // card fill
constexpr uint16_t kUiCardDark = TFT_GRAY_1;   // overdue card fill
constexpr uint16_t kUiCardDone = TFT_GRAY_3;   // done card fill (= page bg, very subtle)
constexpr uint16_t kUiText     = TFT_GRAY_0;
constexpr uint16_t kUiTextInv  = TFT_GRAY_3;
constexpr uint16_t kUiMuted    = TFT_GRAY_1;
constexpr uint16_t kUiLine     = TFT_GRAY_1;
constexpr uint16_t kUiBadge    = TFT_GRAY_2;
#elif VM_SCREEN_MODE == VM_SCREEN_COLOR6
constexpr uint16_t kUiBg       = TFT_WHITE;
constexpr uint16_t kUiCard     = TFT_YELLOW;
constexpr uint16_t kUiCardDark = TFT_RED;
constexpr uint16_t kUiCardDone = TFT_WHITE;
constexpr uint16_t kUiText     = TFT_BLACK;
constexpr uint16_t kUiTextInv  = TFT_WHITE;
constexpr uint16_t kUiMuted    = TFT_BLUE;
constexpr uint16_t kUiLine     = TFT_BLACK;
constexpr uint16_t kUiBadge    = TFT_BLUE;
#elif VM_SCREEN_MODE == VM_SCREEN_GRAY16
constexpr uint16_t kUiBg       = TFT_GRAY_15;
constexpr uint16_t kUiCard     = TFT_GRAY_13;
constexpr uint16_t kUiCardDark = TFT_GRAY_4;
constexpr uint16_t kUiCardDone = TFT_GRAY_14;  // done card almost blends in
constexpr uint16_t kUiText     = TFT_GRAY_0;
constexpr uint16_t kUiTextInv  = TFT_GRAY_14;
constexpr uint16_t kUiMuted    = TFT_GRAY_7;
constexpr uint16_t kUiLine     = TFT_GRAY_8;
constexpr uint16_t kUiBadge    = TFT_GRAY_3;
#endif

const char* kWeekdayShort[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* kMonthShort[]   = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

}  // namespace

MemoUI::MemoUI()
  : display_(),
    checkboxHits_{}
{
  for (size_t i = 0; i < MemoStore::kMax; i++) checkboxHits_[i].valid = false;
}

void MemoUI::begin()
{
  display_.begin();
#if VM_SCREEN_MODE == VM_SCREEN_GRAY4
  display_.initGrayMode(GRAY_LEVEL4);
#elif VM_SCREEN_MODE == VM_SCREEN_GRAY16
  display_.initGrayMode(GRAY_LEVEL16);
#endif
  renderer_.begin(display_);
}

uint16_t MemoUI::displayWidth()  { return static_cast<uint16_t>(display_.width()); }
uint16_t MemoUI::displayHeight() { return static_cast<uint16_t>(display_.height()); }

String MemoUI::formatDueLabel(time_t nowEpoch, const MemoEntry& entry,
                              String& outDateChip, String& outTimeBig,
                              bool& outOverdue)
{
  outOverdue = false;
  if (!entry.hasDue || entry.dueEpoch <= 0 || nowEpoch <= 0) {
    outDateChip = uiStr(UiStringId::kSomeDay);
    outTimeBig  = "--:--";
    return outDateChip + " " + outTimeBig;
  }
  if (entry.dueEpoch < nowEpoch) {
    outOverdue = true;
    outDateChip = uiStr(UiStringId::kOverdue);
    struct tm t = {};
    localtime_r(&entry.dueEpoch, &t);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    outTimeBig = buf;
    return outDateChip + " " + outTimeBig;
  }

  struct tm t = {};
  localtime_r(&entry.dueEpoch, &t);

  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);
  outTimeBig = timeBuf;

  const int days = vmDayDistance(nowEpoch, entry.dueEpoch);
  const char* chipLabel = vmDateChipLabel(days, t.tm_wday);
  if (chipLabel) {
    outDateChip = chipLabel;
  } else {
    char d[16];
#if VM_LANG_ZH
    snprintf(d, sizeof(d), "%d月%d日", t.tm_mon + 1, t.tm_mday);
#else
    snprintf(d, sizeof(d), "%s %02d",
             kMonthShort[(t.tm_mon >= 0 && t.tm_mon < 12) ? t.tm_mon : 0],
             t.tm_mday);
#endif
    outDateChip = d;
  }
  return outDateChip + " " + outTimeBig;
}

void MemoUI::drawWrapped(const String& text, int x, int y, int maxW,
                         int lineH, int textSize, uint16_t color, int maxLines)
{
  auto utf8Len = [&](size_t i) -> size_t {
    const uint8_t c = static_cast<uint8_t>(text[i]);
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
  };

  String line;
  int lines = 0;
  for (size_t i = 0; i < static_cast<size_t>(text.length()) && lines < maxLines; ) {
    const size_t n = utf8Len(i);
    const String token = text.substring(i, i + n);
    i += n;
    if (token == "\r") continue;
    if (token == "\n") {
      renderer_.drawText(line, x, y + lines * lineH, textSize,
                         TextAlign::TopLeft, color, kUiBg);
      line = "";
      lines++;
      continue;
    }
    const String candidate = line + token;
    if (line.length() > 0 && renderer_.measureText(candidate, textSize) > maxW) {
      renderer_.drawText(line, x, y + lines * lineH, textSize,
                         TextAlign::TopLeft, color, kUiBg);
      line = token;
      lines++;
    } else {
      line = candidate;
    }
  }
  if (lines < maxLines && line.length() > 0) {
    renderer_.drawText(line, x, y + lines * lineH, textSize,
                       TextAlign::TopLeft, color, kUiBg);
  }
}

void MemoUI::drawWrappedRight(const String& text, int rightX, int bottomY,
                              int maxW, int lineH, int textSize,
                              uint16_t color, int maxLines)
{
  auto utf8Len = [&](size_t i) -> size_t {
    const uint8_t c = static_cast<uint8_t>(text[i]);
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
  };

  String lines[2];
  const int cappedLines = min(maxLines, 2);
  int count = 0;
  String line;
  for (size_t i = 0; i < static_cast<size_t>(text.length()) && count < cappedLines; ) {
    const size_t n = utf8Len(i);
    const String token = text.substring(i, i + n);
    i += n;
    if (token == "\r" || token == "\n") continue;

    const String candidate = line + token;
    if (line.length() > 0 && renderer_.measureText(candidate, textSize) > maxW) {
      lines[count++] = line;
      line = token;
    } else {
      line = candidate;
    }
  }
  if (count < cappedLines && line.length() > 0) lines[count++] = line;

  for (int i = 0; i < count; i++) {
    const int y = bottomY - (count - 1 - i) * lineH;
    renderer_.drawText(lines[i], rightX, y, textSize,
                       TextAlign::BottomRight, color, kUiBg);
  }
}

void MemoUI::drawStatus(const char* badge, const String& title, const String& body,
                        const String& hint, bool recording, float seconds)
{
  const int w = display_.width();
  const int h = display_.height();
  const int margin = max(24, w / 28);
  const int topH = max(72, h / 8);
  const int titleSize = (w >= 1200) ? 5 : 3;
  const int bodySize  = (w >= 1200) ? 4 : 3;
  const int smallSize = (w >= 1200) ? 3 : 2;

  display_.fillSprite(kUiBg);
  display_.fillRect(0, 0, w, topH, kUiCard);
  display_.drawFastHLine(0, topH - 1, w, kUiLine);

  renderer_.drawText(uiStr(UiStringId::kAppName), margin, topH / 2 - smallSize * 8, smallSize,
                     TextAlign::TopLeft, kUiText, kUiCard);
  renderer_.drawText(VM_DEVICE_NAME, margin, topH / 2 + smallSize * 2, smallSize,
                     TextAlign::TopLeft, kUiMuted, kUiCard);

  {
    const int padX = 18;
    const int bh = 36;
    const int bw = renderer_.measureText(badge, smallSize) + padX * 2;
    const int bx = w - margin - bw;
    const int by = topH / 2 - bh / 2;
    display_.fillRoundRect(bx, by, bw, bh, 10,
                           recording ? kUiCardDark : kUiBadge);
    renderer_.drawText(badge, bx + bw / 2, by + bh / 2 - 1, smallSize,
                       TextAlign::MiddleCenter,
                       recording ? kUiTextInv : kUiText,
                       recording ? kUiCardDark : kUiBadge);
  }

  renderer_.drawText(title, margin, topH + margin, titleSize,
                     TextAlign::TopLeft, kUiText, kUiBg);

  if (recording) {
    const int cx = w / 2;
    const int cy = (topH + h) / 2;
    for (int r = 36; r <= 120; r += 38) display_.drawCircle(cx, cy, r, kUiLine);
    display_.fillCircle(cx, cy, 26, kUiText);
    renderer_.drawText(String(seconds, 1) + "s", cx, cy + 156,
                       (w >= 1200) ? 5 : 3, TextAlign::MiddleCenter,
                       kUiText, kUiBg);
  } else {
    const int textY = topH + margin + titleSize * 10 + 24;
    const int lineH = (bodySize == 4) ? 46 : 34;
    drawWrapped(body, margin, textY, w - margin * 2, lineH, bodySize, kUiText,
                max(3, (h - textY - 100) / lineH));
  }

  renderer_.drawText(hint, margin, h - margin - 24, smallSize,
                     TextAlign::TopLeft, kUiMuted, kUiBg);
  display_.update();
}

void MemoUI::drawClipboardLogo(int x, int y, int size, uint16_t color)
{
  // Clip tab at top center.
  const int clipW = size / 3;
  const int clipH = size / 8;
  display_.fillRoundRect(x + size / 2 - clipW / 2, y, clipW, clipH, 3, color);

  // Board body (double outline so it reads on ePaper).
  const int boardY = y + clipH / 2;
  const int boardH = size - clipH / 2;
  display_.drawRoundRect(x,     boardY,     size,     boardH,     6, color);
  display_.drawRoundRect(x + 1, boardY + 1, size - 2, boardH - 2, 6, color);

  // Ruled lines.
  const int lx1 = x + size / 5;
  const int lx2 = x + size * 4 / 5;
  for (int i = 1; i <= 3; i++) {
    const int ly = boardY + boardH * i / 4;
    const int len = (i == 3) ? (lx2 - lx1) * 2 / 3 : (lx2 - lx1);
    display_.drawFastHLine(lx1, ly, len, color);
  }
}

void MemoUI::drawBatteryIcon(int x, int y, int w, int h, int percent, uint16_t color)
{
  display_.drawRect(x, y, w, h, color);
  const int nubW = 3, nubH = h / 3;
  display_.fillRect(x + w, y + (h - nubH) / 2, nubW, nubH, color);
  if (percent < 0) return;                       // unknown -> empty body
  const int fillW = (w - 4) * percent / 100;
  if (fillW > 0) display_.fillRect(x + 2, y + 2, fillW, h - 4, color);
}

void MemoUI::drawWifiIcon(int x, int y, int w, int h, bool connected, uint16_t color)
{
  // Signal-bar style (ascending). Solid = connected, outline + slash = not.
  const int bars = 4, gap = 3;
  const int bw = (w - gap * (bars - 1)) / bars;
  for (int i = 0; i < bars; i++) {
    const int bh = h * (i + 1) / bars;
    const int bx = x + i * (bw + gap);
    const int by = y + h - bh;
    if (connected) display_.fillRect(bx, by, bw, bh, color);
    else           display_.drawRect(bx, by, bw, bh, color);
  }
  if (!connected) display_.drawLine(x, y, x + w, y + h, color);
}

void MemoUI::drawCheckbox(int cx, int cy, int size, bool done, uint16_t fg)
{
  const int x = cx - size / 2;
  const int y = cy - size / 2;
  if (done) {
    // Filled square with a white check mark inside.
    display_.fillRoundRect(x, y, size, size, 6, fg);
    // Check mark: two strokes that meet at the bottom-left.
    const int ax = x + size * 22 / 100, ay = y + size * 52 / 100;
    const int bx = x + size * 42 / 100, by = y + size * 72 / 100;
    const int dx = x + size * 78 / 100, dy = y + size * 28 / 100;
    // Draw twice with +/- offset so the stroke is thick enough to read on
    // ePaper without needing a fancy line-width primitive.
    for (int d = -1; d <= 1; d++) {
      display_.drawLine(ax, ay + d, bx, by + d, kUiBg);
      display_.drawLine(bx, by + d, dx, dy + d, kUiBg);
      display_.drawLine(ax + d, ay, bx + d, by, kUiBg);
      display_.drawLine(bx + d, by, dx + d, dy, kUiBg);
    }
  } else {
    display_.drawRoundRect(x, y,     size,     size,     6, fg);
    display_.drawRoundRect(x + 1, y + 1, size - 2, size - 2, 6, fg);
  }
}

void MemoUI::drawCard(int x, int y, int w, int h,
                      const MemoEntry& entry, time_t nowEpoch,
                      HitRect& outHit)
{
  // Pick palette based on done / overdue state.
  uint16_t fill;
  uint16_t fg;
  if (entry.done) {
    fill = kUiCardDone;
    fg   = kUiMuted;       // grayed text for completed items
  } else if (entry.hasDue && nowEpoch > 0 && entry.dueEpoch < nowEpoch) {
    fill = kUiCardDark;
    fg   = kUiTextInv;
  } else {
    fill = kUiCard;
    fg   = kUiText;
  }
  const bool overdue = (fill == kUiCardDark);

  display_.fillRoundRect(x, y, w, h, 14, fill);

  // ---- Checkbox (left, vertically centered) ----
  const int boxSize = 56;
  const int boxCx   = x + 24 + boxSize / 2;
  const int boxCy   = y + h / 2;
  drawCheckbox(boxCx, boxCy, boxSize, entry.done, fg);

  // Generous hit zone: the entire left strip of the card, not just the
  // visible box, so a tap with a fingertip is easy to land.
  outHit.x = x;
  outHit.y = y;
  outHit.w = 24 + boxSize + 24;   // up to the start of the memo text
  outHit.h = h;
  outHit.valid = true;

  // Layout common to both right-side rendering modes.
  const int memoX    = x + 24 + boxSize + 24;
  const int rightPad = 28;
  const int rightW   = 360;
  const int memoMaxW = (x + w - rightPad - rightW) - memoX;

  // ---- Right side: date chip (top) + big time / time-of-day (bottom) ----
  {
    String dateChip, timeBig;
    bool   over = false;
    formatDueLabel(nowEpoch, entry, dateChip, timeBig, over);

    if (entry.fuzzyLabel == "NONE") {
      timeBig = "";                 // user gave no time -> leave blank
    } else if (entry.fuzzyLabel.length() > 0) {
      timeBig = entry.fuzzyLabel;   // time-of-day word (Morning/Evening/...)
    }

    const int rightEdge = x + w - rightPad;

    // Date chip near the top of the card.
    const int chipPad = 18;
    const int chipH   = 38;
    const int chipW   = renderer_.measureText(dateChip, 3) + chipPad * 2;
    const int chipX   = rightEdge - chipW;
    const int chipY   = y + 16;
    const uint16_t chipFill = overdue ? kUiCardDark
                               : (entry.done ? kUiMuted : kUiBadge);
    display_.fillRoundRect(chipX, chipY, chipW, chipH, 8, chipFill);
    renderer_.drawText(dateChip, chipX + chipW / 2, chipY + chipH / 2 - 1, 3,
                       TextAlign::MiddleCenter, kUiTextInv, chipFill);

    // Big time anchored to the BOTTOM of the card, so it can never overlap
    // the chip regardless of card height.
    if (timeBig.length() > 0) {
      renderer_.drawText(timeBig, rightEdge, y + h - 16, 5,
                         TextAlign::BottomRight, fg, fill);
    }
  }

  // ---- Memo text ----
  const int memoSize  = 4;
  const int memoLineH = 42;
  const int oneLineW  = renderer_.measureText(entry.text, memoSize);

  if (oneLineW <= memoMaxW) {
    renderer_.drawText(entry.text, memoX, y + h / 2 - memoSize * 6, memoSize,
                       TextAlign::TopLeft, fg, fill);
  } else {
    drawWrapped(entry.text, memoX, y + 24, memoMaxW, memoLineH,
                memoSize, fg, 2);
  }
}

void MemoUI::drawCompactCard(int x, int y, int w, int h,
                             const MemoEntry& entry, time_t nowEpoch,
                             HitRect& outHit)
{
  uint16_t fill;
  uint16_t fg;
  if (entry.done) {
    fill = kUiCardDone;
    fg   = kUiMuted;
  } else if (entry.hasDue && nowEpoch > 0 && entry.dueEpoch < nowEpoch) {
    fill = kUiCardDark;
    fg   = kUiText;
  } else {
    fill = kUiCard;
    fg   = kUiText;
  }
  const bool overdue = (fill == kUiCardDark);

  display_.fillRoundRect(x, y, w, h, 6, fill);

  const int boxSize = 22;
  const int boxCx = x + 12 + boxSize / 2;
  const int boxCy = y + h / 2;
  drawCheckbox(boxCx, boxCy, boxSize, entry.done, fg);

  // Small panels have no touch layer; keep the visual checkbox only.
  // 小屏设备没有触摸层；这里只保留视觉上的复选框。
  outHit.valid = false;

  const int memoX = x + 12 + boxSize + 10;
  const int rightPad = 10;
  const int rightW = 122;
  const int memoMaxW = max(80, (x + w - rightPad - rightW) - memoX);

  String dateChip, timeBig;
  bool over = false;
  formatDueLabel(nowEpoch, entry, dateChip, timeBig, over);
  if (entry.fuzzyLabel == "NONE") {
    timeBig = "";
  } else if (entry.fuzzyLabel.length() > 0) {
    timeBig = entry.fuzzyLabel;
  }

  const int rightEdge = x + w - rightPad;
  const int chipPad = 6;
  const int chipH = 25;
  const int chipSize = 2;
  const int chipW = min(rightW, renderer_.measureText(dateChip, chipSize) + chipPad * 2);
  const int chipX = rightEdge - chipW;
  const int chipY = y + 7;
  const uint16_t chipFill = overdue ? kUiCardDark
                             : (entry.done ? kUiMuted : kUiBadge);
  display_.fillRoundRect(chipX, chipY, chipW, chipH, 4, chipFill);
  renderer_.drawText(dateChip, chipX + chipW / 2, chipY + chipH / 2,
                     chipSize, TextAlign::MiddleCenter, kUiText, chipFill);

  if (timeBig.length() > 0) {
    renderer_.drawText(timeBig, rightEdge, y + h - 8, 2,
                       TextAlign::BottomRight, fg, fill);
  }

  const int memoSize = 2;
  const int memoLineH = 18;
  const int oneLineW = renderer_.measureText(entry.text, memoSize);
  if (oneLineW <= memoMaxW) {
    renderer_.drawText(entry.text, memoX, y + h / 2 - 8, memoSize,
                       TextAlign::TopLeft, fg, fill);
  } else {
    drawWrapped(entry.text, memoX, y + 12, memoMaxW, memoLineH,
                memoSize, fg, 2);
  }
}

void MemoUI::drawHeader(RtcClock& rtc, const UiStatus& st)
{
  const int w = display_.width();
  const int margin = 70;
  const int topY = 30;

  // Left column: clipboard logo (top) + "Notes" (bottom), enlarged.
  const int logoSize = 72;
  drawClipboardLogo(margin, topY, logoSize, kUiText);
  renderer_.drawText(uiStr(UiStringId::kAppName), margin, topY + logoSize + 10, 4,
                     TextAlign::TopLeft, kUiText, kUiBg);

  // Center column: big time (top) + date (bottom), centered and enlarged.
  renderer_.drawText(rtc.nowTimeLabel(), w / 2, topY, 11,
                     TextAlign::TopCenter, kUiText, kUiBg);
  renderer_.drawText(rtc.nowHeaderDateLabel(), w / 2, topY + 11 * 8 + 18, 4,
                     TextAlign::TopCenter, kUiText, kUiBg);

  // Right column.
  if (st.processing) {
    const int cx = w - margin - 24;
    const int cy = topY + 26;
    display_.drawCircle(cx, cy, 22, kUiText);
    display_.fillCircle(cx, cy - 22, 5, kUiText);
    renderer_.drawText(uiStr(UiStringId::kProcessing), cx - 40, cy, 4,
                       TextAlign::MiddleRight, kUiText, kUiBg);
  } else {
    const int battW = 60, battH = 30;
    const int battX = w - margin - battW - 4;
    const int battY = topY + 6;
    drawBatteryIcon(battX, battY, battW, battH, st.batteryPercent, kUiText);
    char pbuf[8];
    snprintf(pbuf, sizeof(pbuf), "%d%%", st.batteryPercent < 0 ? 0 : st.batteryPercent);
    renderer_.drawText(pbuf, battX - 8, battY + battH / 2, 3,
                       TextAlign::MiddleRight, kUiText, kUiBg);

    const int wifiW = 44, wifiH = 32;
    drawWifiIcon(w - margin - wifiW, battY + battH + 18, wifiW, wifiH,
                 st.wifiConnected, kUiText);
  }
}

void MemoUI::drawCompactHeader(RtcClock& rtc, const UiStatus& st)
{
  const int w = display_.width();
  const int margin = 16;
  const int topY = 10;

  const int logoSize = 28;
  drawClipboardLogo(margin, topY, logoSize, kUiText);
  renderer_.drawText(uiStr(UiStringId::kAppName), margin + logoSize + 8, topY, 2,
                     TextAlign::TopLeft, kUiText, kUiBg);
  renderer_.drawText(VM_DEVICE_NAME, margin + logoSize + 8, topY + 23, 1,
                     TextAlign::TopLeft, kUiMuted, kUiBg);

  renderer_.drawText(rtc.nowTimeLabel(), w / 2, topY - 1, 4,
                     TextAlign::TopCenter, kUiText, kUiBg);
  renderer_.drawText(rtc.nowHeaderDateLabel(), w / 2, topY + 39, 2,
                     TextAlign::TopCenter, kUiText, kUiBg);

  if (st.processing) {
    renderer_.drawText(uiStr(UiStringId::kProcessing), w - margin, topY + 18, 2,
                       TextAlign::MiddleRight, kUiText, kUiBg);
    return;
  }

  const int battW = 32;
  const int battH = 14;
  const int battX = w - margin - battW - 3;
  const int battY = topY + 4;
  drawBatteryIcon(battX, battY, battW, battH, st.batteryPercent, kUiText);
  char pbuf[8];
  snprintf(pbuf, sizeof(pbuf), "%d%%", st.batteryPercent < 0 ? 0 : st.batteryPercent);
  renderer_.drawText(pbuf, battX - 5, battY + battH / 2, 1,
                     TextAlign::MiddleRight, kUiText, kUiBg);

  drawWifiIcon(w - margin - 24, topY + 29, 24, 18,
               st.wifiConnected, kUiText);
}

void MemoUI::drawBoot(RtcClock& rtc, const String& statusText, const UiStatus& st)
{
  display_.fillSprite(kUiBg);
#if VM_SCREEN_MODE == VM_SCREEN_GRAY16
  drawHeader(rtc, st);
  const int statusSize = 4;
#else
  drawCompactHeader(rtc, st);
  const int statusSize = 2;
#endif
  renderer_.drawText(statusText, display_.width() / 2, display_.height() / 2,
                     statusSize, TextAlign::MiddleCenter, kUiText, kUiBg);
  display_.update();
}

void MemoUI::drawTodoList(MemoStore& store, RtcClock& rtc,
                          const UiStatus& status, const String& hint,
                          const String& quote)
{
  const time_t nowEpoch = rtc.nowEpoch();
  store.sortByDue(nowEpoch);

  // Reset hit cache. Cards that get drawn refill their slot.
  for (size_t i = 0; i < MemoStore::kMax; i++) checkboxHits_[i].valid = false;

#if VM_SCREEN_MODE == VM_SCREEN_GRAY16
  const int w = display_.width();
  const int h = display_.height();
  const int margin = 80;

  display_.fillSprite(kUiBg);
  drawHeader(rtc, status);

  const int headerH = 180;   // header band height (no divider line)

  // ---- Card list ----
  const int footerH = 100;
  const int listTop = headerH + 24;
  const int listBottom = h - footerH;
  const int listAvail = listBottom - listTop;
  const int rowH = listAvail / static_cast<int>(MemoStore::kMax);
  const int gap = 8;
  const int cardH = rowH - gap;
  const int cardW = w - margin * 2;

  if (store.count() == 0) {
    renderer_.drawText(uiStr(UiStringId::kEmptyList), w / 2,
                       listTop + listAvail / 2, 4,
                       TextAlign::MiddleCenter, kUiMuted, kUiBg);
  } else {
    for (size_t i = 0; i < store.count(); i++) {
      const int cardY = listTop + static_cast<int>(i) * rowH + gap / 2;
      drawCard(margin, cardY, cardW, cardH, store.at(i), nowEpoch,
               checkboxHits_[i]);
    }
  }

  // ---- Footer ----
  const int footerTextSize = 3;
  const int footerY = h - 36;
  const int quoteW = w - margin * 2 - renderer_.measureText(hint, footerTextSize) - 56;
  renderer_.drawText(hint, margin, footerY, footerTextSize,
                     TextAlign::BottomLeft, kUiText, kUiBg);
  if (quote.length() > 0 && quoteW > 80) {
    drawWrappedRight(quote, w - margin, footerY, quoteW, 34,
                     footerTextSize, kUiText, 2);
  }
  display_.update();
#else
  const int w = display_.width();
  const int h = display_.height();
  const int margin = 16;
  const int headerH = 74;
  const int footerH = 34;
  const int listTop = headerH + 8;
  const int listBottom = h - footerH;
  const int visibleMax = min(static_cast<int>(VM_VISIBLE_MEMO_MAX),
                             static_cast<int>(MemoStore::kMax));
  const int rowH = max(1, (listBottom - listTop) / visibleMax);
  const int gap = 5;
  const int cardH = max(44, rowH - gap);
  const int cardW = w - margin * 2;

  display_.fillSprite(kUiBg);
  drawCompactHeader(rtc, status);

  if (store.count() == 0) {
    renderer_.drawText(uiStr(UiStringId::kEmptyList), w / 2,
                       listTop + (listBottom - listTop) / 2, 2,
                       TextAlign::MiddleCenter, kUiMuted, kUiBg);
  } else {
    const size_t renderCount = min(store.count(), static_cast<size_t>(visibleMax));
    for (size_t i = 0; i < renderCount; i++) {
      const int cardY = listTop + static_cast<int>(i) * rowH + gap / 2;
      drawCompactCard(margin, cardY, cardW, cardH, store.at(i), nowEpoch,
                      checkboxHits_[i]);
    }
  }

  renderer_.drawText(hint, margin, h - 8, 1,
                     TextAlign::BottomLeft, kUiMuted, kUiBg);
  if (quote.length() > 0) {
    drawWrappedRight(quote, w - margin, h - 8, w / 2, 16, 1, kUiMuted, 1);
  }
  display_.update();
#endif
}

int MemoUI::hitTestCheckbox(int x, int y) const
{
  for (size_t i = 0; i < MemoStore::kMax; i++) {
    const HitRect& r = checkboxHits_[i];
    if (!r.valid) continue;
    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
      return static_cast<int>(i);
    }
  }
  return -1;
}
