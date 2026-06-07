// TextRenderer.h -- text rendering facade for the UI.
//
// MemoUI draws every string through drawText() / measureText() instead of
// touching the panel font API directly. The English build routes these to the
// built-in Seeed_GFX bitmap font; the Chinese build (VM_LANG_ZH) routes them to
// OpenFontRender with a TrueType font stored in SPIFFS. The English build never
// includes or links OpenFontRender, so it carries no new risk.

#ifndef VOICE_MEMO_TEXT_RENDERER_H
#define VOICE_MEMO_TEXT_RENDERER_H

#include <Arduino.h>

#include "TFT_eSPI.h"

// The anchor of the text box that the (x, y) given to drawText refers to.
enum class TextAlign {
  TopLeft, TopCenter, TopRight,
  MiddleLeft, MiddleCenter, MiddleRight,
  BottomLeft, BottomCenter, BottomRight
};

class TextRenderer {
 public:
  // English build: always succeeds (the bitmap font is built in).
  // Chinese build: mounts SPIFFS, binds the OpenFontRender drawer to the
  // display, and loads the .ttf. Returns false and leaves fontReady() false
  // if the font cannot be loaded.
  bool begin(EPaper& display);

  bool fontReady() const { return fontReady_; }

  // Draws `text` so the chosen anchor lands at (x, y). `sizeUnit` keeps the
  // bitmap setTextSize() scale; the Chinese path maps it to pixels via a
  // single tunable constant. `bg` is used for anti-aliased edge blending.
  void drawText(const String& text, int x, int y, int sizeUnit,
                TextAlign align, uint16_t color, uint16_t bg);

  // Pixel width of `text` at `sizeUnit`, for layout that needs to measure.
  int measureText(const String& text, int sizeUnit);

 private:
  EPaper* display_ = nullptr;
  bool fontReady_ = false;
};

#endif  // VOICE_MEMO_TEXT_RENDERER_H
