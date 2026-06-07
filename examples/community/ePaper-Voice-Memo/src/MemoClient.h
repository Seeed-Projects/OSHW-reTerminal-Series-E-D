// MemoClient.h -- turn a raw transcript into a MemoEntry.
//
// The transcript that comes back from speech-to-text may be Chinese, English
// or mixed, may be long, may include filler words. This module wraps it in a
// chat/completions call to an OpenAI-compatible LLM and asks for a STRICT
// JSON answer:
//
//   {"memo":"<one English sentence>","due":"YYYY-MM-DD HH:MM"}
//
// The LLM also receives the device's current local time so phrases like
// "tomorrow morning" or "in two hours" can be resolved to an absolute time.
//
// On any failure (no WiFi, HTTP error, malformed JSON, unparseable due time),
// the module degrades gracefully: it returns a MemoEntry built from a small
// on-device rule and falls back to "now + 1 hour" as the due time. This keeps
// the sort order stable and the persistence layer happy.

#ifndef VOICE_MEMO_MEMO_CLIENT_H
#define VOICE_MEMO_MEMO_CLIENT_H

#include <Arduino.h>
#include <time.h>

#include "MemoStore.h"  // MemoEntry

enum VoiceMemoMemoProvider {
  VM_MEMO_RULE,
  VM_MEMO_OPENAI_COMPATIBLE,
};

struct MemoConfig {
  VoiceMemoMemoProvider provider;
  const char* url;
  const char* apiKey;
  const char* model;
};

class MemoClient {
 public:
  MemoClient();

  void configure(const MemoConfig& cfg, uint32_t httpTimeoutMs);

  // Always returns a valid MemoEntry. nowEpoch is the device's current local
  // time and is used both for the LLM "today" reference and for the fallback
  // due time. The caller must ensure WiFi is connected when provider is
  // VM_MEMO_OPENAI_COMPATIBLE.
  MemoEntry summarize(const String& transcript, time_t nowEpoch);

 private:
  MemoEntry summarizeOpenAICompatible(const String& transcript, time_t nowEpoch);
  MemoEntry summarizeWithRule(const String& transcript, time_t nowEpoch);

  // Parses "YYYY-MM-DD HH:MM" (with or without seconds). Returns 0 on
  // failure or any value below the year 2020 sanity floor.
  time_t parseLocalDateTime(const String& text);
  MemoEntry buildFallback(const String& transcript, time_t nowEpoch);

  MemoConfig cfg_;
  uint32_t   httpTimeoutMs_;
};

#endif  // VOICE_MEMO_MEMO_CLIENT_H
