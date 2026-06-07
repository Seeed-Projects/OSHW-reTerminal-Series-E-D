// DisplayText.h -- final text cleanup before strings reach the e-paper UI.
//
// The Chinese font subset intentionally focuses on letters and glyphs used by
// reminders, so punctuation from STT/LLM output is converted to spaces before
// the text is saved or drawn.

#ifndef VOICE_MEMO_DISPLAY_TEXT_H
#define VOICE_MEMO_DISPLAY_TEXT_H

#include <ctype.h>
#include <stddef.h>
#if defined(ARDUINO)
  #include <Arduino.h>
#else
  #include <string>
#endif

#include "UiLang.h"

inline bool vmIsDisplayPunctuation(uint32_t cp)
{
  switch (cp) {
    case 0x00B7:  // middle dot
    case 0x2014:  // em dash
    case 0x2018:  // left single quote
    case 0x2019:  // right single quote
    case 0x201C:  // left double quote
    case 0x201D:  // right double quote
    case 0x2026:  // ellipsis
    case 0x3000:  // ideographic space
    case 0x3001:  // ideographic comma
    case 0x3002:  // ideographic full stop
    case 0x3008:  // left angle bracket
    case 0x3009:  // right angle bracket
    case 0x300A:  // left double angle bracket
    case 0x300B:  // right double angle bracket
    case 0x3010:  // left black lenticular bracket
    case 0x3011:  // right black lenticular bracket
    case 0xFF01:  // fullwidth exclamation mark
    case 0xFF08:  // fullwidth left parenthesis
    case 0xFF09:  // fullwidth right parenthesis
    case 0xFF0C:  // fullwidth comma
    case 0xFF0D:  // fullwidth hyphen-minus
    case 0xFF0E:  // fullwidth full stop
    case 0xFF1A:  // fullwidth colon
    case 0xFF1B:  // fullwidth semicolon
    case 0xFF1F:  // fullwidth question mark
    case 0xFF5E:  // fullwidth tilde
      return true;
    default:
      return false;
  }
}

inline size_t vmUtf8TokenLen(const char* text, size_t len, size_t i,
                             uint32_t& cp)
{
  const uint8_t c0 = static_cast<uint8_t>(text[i]);
  cp = c0;
  if ((c0 & 0x80) == 0) return 1;

  const size_t remaining = len - i;
  if ((c0 & 0xE0) == 0xC0 && remaining >= 2) {
    const uint8_t c1 = static_cast<uint8_t>(text[i + 1]);
    cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
    return 2;
  }
  if ((c0 & 0xF0) == 0xE0 && remaining >= 3) {
    const uint8_t c1 = static_cast<uint8_t>(text[i + 1]);
    const uint8_t c2 = static_cast<uint8_t>(text[i + 2]);
    cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
    return 3;
  }
  if ((c0 & 0xF8) == 0xF0 && remaining >= 4) {
    const uint8_t c1 = static_cast<uint8_t>(text[i + 1]);
    const uint8_t c2 = static_cast<uint8_t>(text[i + 2]);
    const uint8_t c3 = static_cast<uint8_t>(text[i + 3]);
    cp = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12)
       | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    return 4;
  }
  return 1;
}

#if defined(ARDUINO)
inline String vmSanitizeDisplayTextForLang(const String& text, bool chinese)
{
  if (!chinese) return text;

  String out;
  out.reserve(text.length());
  bool pendingSpace = false;
  for (size_t i = 0; i < static_cast<size_t>(text.length()); ) {
    uint32_t cp = 0;
    const size_t n = vmUtf8TokenLen(text.c_str(), text.length(), i, cp);
    const bool ascii = cp < 0x80;
    const bool replace = (ascii && (isspace(static_cast<int>(cp))
                         || ispunct(static_cast<int>(cp))))
                      || vmIsDisplayPunctuation(cp);
    if (replace) {
      pendingSpace = out.length() > 0;
    } else {
      if (pendingSpace) {
        out += ' ';
        pendingSpace = false;
      }
      out += text.substring(i, i + n);
    }
    i += n;
  }
  out.trim();
  return out;
}

inline String vmSanitizeDisplayText(const String& text)
{
  return vmSanitizeDisplayTextForLang(text, VM_LANG_ZH != 0);
}
#else
inline std::string vmSanitizeDisplayTextForLang(const std::string& text,
                                                bool chinese)
{
  if (!chinese) return text;

  std::string out;
  out.reserve(text.length());
  bool pendingSpace = false;
  for (size_t i = 0; i < text.length(); ) {
    uint32_t cp = 0;
    const size_t n = vmUtf8TokenLen(text.c_str(), text.length(), i, cp);
    const bool ascii = cp < 0x80;
    const bool replace = (ascii && (isspace(static_cast<int>(cp))
                         || ispunct(static_cast<int>(cp))))
                      || vmIsDisplayPunctuation(cp);
    if (replace) {
      pendingSpace = !out.empty();
    } else {
      if (pendingSpace) {
        out += ' ';
        pendingSpace = false;
      }
      out.append(text, i, n);
    }
    i += n;
  }
  while (!out.empty() && out[out.length() - 1] == ' ') out.pop_back();
  return out;
}

inline std::string vmSanitizeDisplayText(const std::string& text)
{
  return vmSanitizeDisplayTextForLang(text, VM_LANG_ZH != 0);
}
#endif

#endif  // VOICE_MEMO_DISPLAY_TEXT_H
