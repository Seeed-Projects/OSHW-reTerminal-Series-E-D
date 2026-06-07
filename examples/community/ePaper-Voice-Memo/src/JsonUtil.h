// JsonUtil.h -- minimal JSON helpers shared by SpeechClient and MemoClient.
//
// This example talks to OpenAI-compatible APIs whose responses are small and
// shaped like {"text":"...","choices":[{"message":{"content":"..."}}]}, so a
// full JSON parser would be heavier than needed. These helpers do exactly two
// things:
//   - jsonStringValue(json, key): return the string value for the first
//     occurrence of "key":"..." in json. Returns "" if the key is missing or
//     the value is null.
//   - jsonEscape(text): escape quotes, backslashes and control characters so
//     a user transcript can be embedded in a JSON request body.
//
// If a future contributor needs richer parsing, swap these helpers for
// ArduinoJson and update the two callers.

#ifndef VOICE_MEMO_JSON_UTIL_H
#define VOICE_MEMO_JSON_UTIL_H

#include <Arduino.h>

namespace voice_memo {

inline String jsonStringValue(const String& json, const char* key)
{
  const String needle = String("\"") + key + "\":";
  int pos = json.indexOf(needle);
  if (pos < 0) return "";

  pos += needle.length();
  // Skip whitespace between ':' and the value.
  while (pos < static_cast<int>(json.length())
         && (json[pos] == ' ' || json[pos] == '\t')) {
    pos++;
  }
  if (pos >= static_cast<int>(json.length())) return "";

  // Literal null -> empty string for callers that treat empty as "missing".
  if (json[pos] != '"') return "";

  String out;
  bool escaped = false;
  for (int i = pos + 1; i < static_cast<int>(json.length()); i++) {
    const char c = json[i];
    if (escaped) {
      if (c == 'n') out += '\n';
      else if (c == 't') out += ' ';
      else out += c;
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c == '"') {
      break;
    } else {
      out += c;
    }
  }
  return out;
}

inline String jsonEscape(const String& text)
{
  String out;
  out.reserve(text.length() + 16);
  for (int i = 0; i < static_cast<int>(text.length()); i++) {
    const char c = text[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n' || c == '\r' || c == '\t') {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out;
}

// Extracts the body of the first JSON object found in text, starting at the
// first '{' and ending at the matching '}'. Used to strip markdown code fences
// or extra prose that some LLMs add around their JSON answer. Returns "" when
// no balanced object is found.
inline String jsonExtractObject(const String& text)
{
  const int start = text.indexOf('{');
  if (start < 0) return "";

  int depth = 0;
  bool inString = false;
  bool escaped = false;
  for (int i = start; i < static_cast<int>(text.length()); i++) {
    const char c = text[i];
    if (inString) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"') inString = true;
    else if (c == '{') depth++;
    else if (c == '}') {
      depth--;
      if (depth == 0) return text.substring(start, i + 1);
    }
  }
  return "";
}

}  // namespace voice_memo

#endif  // VOICE_MEMO_JSON_UTIL_H
