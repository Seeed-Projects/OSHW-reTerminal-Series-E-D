// MemoUI.h -- all e-paper rendering for the voice memo app.
//
// Responsibilities:
//   - Initialize the EPaper sprite at boot and choose a 4 / 6 / 16 level
//     palette that matches the device defined in driver.h.
//   - Render the main reminder list as cards. E1003 uses the full-size layout;
//     E1001 / E1002 use the same visual idea with smaller metrics.
//   - Render the boot / recording / processing / error status screens.
//   - Format a MemoEntry's due time into a short, human label such as
//     "Today 18:00" or "Overdue".
//
// All ePaper refreshes happen inside the public draw* methods. The class
// never holds long-lived references to MemoStore or RtcClock so a future
// contributor can render off-screen test snapshots without owning the chip.

#ifndef VOICE_MEMO_MEMO_UI_H
#define VOICE_MEMO_MEMO_UI_H

#include <Arduino.h>
#include <time.h>

#include "driver.h"
#include "MemoStore.h"
#include "RtcClock.h"
#include "TextRenderer.h"
#include "TFT_eSPI.h"

// Snapshot of device status drawn in the header.
struct UiStatus {
  bool wifiConnected = false;
  int  batteryPercent = -1;   // -1 = unknown
  bool processing = false;    // true -> show "Processing" instead of icons
};

class MemoUI {
 public:
  MemoUI();

  void begin();

  // Native display dimensions, used by TouchInput to scale coordinates.
  uint16_t displayWidth();
  uint16_t displayHeight();

  // Status pages used during boot, recording, processing and errors.
  void drawStatus(const char* badge,
                  const String& title,
                  const String& body,
                  const String& hint,
                  bool recording,
                  float seconds);

  // Main reminder list. nowEpoch is used to label entries as Today /
  // Tomorrow / Overdue and to sort the cards in MemoStore before rendering.
  // Side effect: also populates the internal checkbox hit-test cache so
  // hitTestCheckbox() can resolve subsequent taps to a row index.
  void drawTodoList(MemoStore& store,
                    RtcClock& rtc,
                    const UiStatus& status,
                    const String& hint,
                    const String& quote);

  // One-page boot/splash screen reusing the header.
  void drawBoot(RtcClock& rtc, const String& statusText, const UiStatus& status);

  // Returns 0..kMax-1 if (x, y) falls inside a card's checkbox hit zone
  // from the most recent drawTodoList() call. Returns -1 otherwise.
  int hitTestCheckbox(int x, int y) const;

  // Exposed mostly for tests / future contributors who want to render a
  // single label in a different layout.
  static String formatDueLabel(time_t nowEpoch,
                               const MemoEntry& entry,
                               String& outDateChip,
                               String& outTimeBig,
                               bool& outOverdue);

 private:
  struct HitRect {
    int x, y, w, h;
    bool valid;
  };

  EPaper display_;
  TextRenderer renderer_;
  HitRect checkboxHits_[MemoStore::kMax];

  // Card layouts.
  void drawCard(int x, int y, int w, int h,
                const MemoEntry& entry, time_t nowEpoch,
                HitRect& outHit);
  void drawCompactCard(int x, int y, int w, int h,
                       const MemoEntry& entry, time_t nowEpoch,
                       HitRect& outHit);
  void drawCheckbox(int cx, int cy, int size, bool done, uint16_t fg);

  // Shared text helpers.
  void drawWrapped(const String& text, int x, int y, int maxW, int lineH,
                   int textSize, uint16_t color, int maxLines);
  void drawWrappedRight(const String& text, int rightX, int bottomY, int maxW,
                        int lineH, int textSize, uint16_t color, int maxLines);

  void drawClipboardLogo(int x, int y, int size, uint16_t color);
  void drawBatteryIcon(int x, int y, int w, int h, int percent, uint16_t color);
  void drawWifiIcon(int x, int y, int w, int h, bool connected, uint16_t color);

  void drawHeader(RtcClock& rtc, const UiStatus& status);
  void drawCompactHeader(RtcClock& rtc, const UiStatus& status);
};

#endif  // VOICE_MEMO_MEMO_UI_H
