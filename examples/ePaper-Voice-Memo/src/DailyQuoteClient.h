// DailyQuoteClient.h -- one short motivational line cached per local day.
//
// The client reuses the OpenAI-compatible memo model configuration, asks for a
// single upbeat quote in the active firmware language, and stores the result in
// NVS so a reboot does not spend another API call on the same day.

#ifndef VOICE_MEMO_DAILY_QUOTE_CLIENT_H
#define VOICE_MEMO_DAILY_QUOTE_CLIENT_H

#include <Arduino.h>
#include <time.h>

#include "MemoClient.h"  // MemoConfig

class DailyQuoteClient {
 public:
  DailyQuoteClient();

  void configure(const MemoConfig& cfg, uint32_t httpTimeoutMs);

  // Loads the cached quote, if any. Safe to call once after configure().
  bool begin(time_t nowEpoch);

  // Refreshes only when the local calendar day changed. Returns true when the
  // visible quote changed, false when the cached/fallback quote is kept.
  bool refreshIfNeeded(time_t nowEpoch, bool allowNetwork);
  bool needsRefresh(time_t nowEpoch) const;

  const String& quote() const { return quote_; }

 private:
  MemoConfig cfg_;
  uint32_t httpTimeoutMs_;
  int cachedDateKey_;
  String quote_;

  static int dateKey(time_t epoch);
  static const char* fallbackQuote();

  bool load();
  bool save();
  bool requestQuote(time_t nowEpoch, String& outQuote);
};

#endif  // VOICE_MEMO_DAILY_QUOTE_CLIENT_H
