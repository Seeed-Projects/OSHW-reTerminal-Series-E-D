#include "DailyQuoteClient.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>

#include "DisplayText.h"
#include "JsonUtil.h"
#include "UiLang.h"

namespace {

constexpr const char* kNvsNamespace = "vmq";
constexpr const char* kNvsDateKey   = "date";
constexpr const char* kNvsLangKey   = "lang";
constexpr const char* kNvsQuoteKey  = "quote";
constexpr size_t kMaxQuoteChars = 72;

bool beginHttp(HTTPClient& http, WiFiClientSecure& secure, const char* url)
{
  const String target(url);
  if (target.startsWith("https://")) {
    secure.setInsecure();
    return http.begin(secure, target);
  }
  return http.begin(target);
}

String formatLocalDate(time_t epoch)
{
  if (epoch <= 0) return "unknown";
  struct tm t = {};
  localtime_r(&epoch, &t);
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  return String(buf);
}

String trimQuote(String quote)
{
  quote.trim();
  if (quote.startsWith("\"") && quote.endsWith("\"") && quote.length() >= 2) {
    quote = quote.substring(1, quote.length() - 1);
    quote.trim();
  }
  if (quote.length() > kMaxQuoteChars) {
    quote = quote.substring(0, kMaxQuoteChars);
    quote.trim();
  }
  return quote;
}

}  // namespace

DailyQuoteClient::DailyQuoteClient()
  : cfg_{},
    httpTimeoutMs_(45000),
    cachedDateKey_(0),
    quote_("")
{
}

void DailyQuoteClient::configure(const MemoConfig& cfg, uint32_t httpTimeoutMs)
{
  cfg_ = cfg;
  httpTimeoutMs_ = httpTimeoutMs;
}

bool DailyQuoteClient::begin(time_t nowEpoch)
{
  const bool loaded = load();
  if (quote_.length() == 0) quote_ = fallbackQuote();
  refreshIfNeeded(nowEpoch, /*allowNetwork=*/false);
  return loaded;
}

int DailyQuoteClient::dateKey(time_t epoch)
{
  if (epoch <= 0) return 0;
  struct tm t = {};
  localtime_r(&epoch, &t);
  return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}

const char* DailyQuoteClient::fallbackQuote()
{
#if VM_LANG_ZH
  return "今天也要稳稳发光 哪怕只是省电模式";
#else
  return "Tiny steps count, especially when they fit on e-paper.";
#endif
}

bool DailyQuoteClient::load()
{
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, /*readOnly=*/false)) return false;

  const uint8_t raw = prefs.getUChar(kNvsLangKey, 0xFF);
  const int storedTag = (raw == 0xFF) ? -1 : static_cast<int>(raw);
  if (vmShouldWipeForLanguage(storedTag, VM_LANG_ZH)) {
    prefs.clear();
    prefs.putUChar(kNvsLangKey, static_cast<uint8_t>(VM_LANG_ZH));
    prefs.end();
    cachedDateKey_ = 0;
    quote_ = "";
    return false;
  }

  cachedDateKey_ = prefs.getInt(kNvsDateKey, 0);
  quote_ = vmSanitizeDisplayText(prefs.getString(kNvsQuoteKey, ""));
  prefs.end();
  return quote_.length() > 0;
}

bool DailyQuoteClient::save()
{
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, /*readOnly=*/false)) return false;
  prefs.putUChar(kNvsLangKey, static_cast<uint8_t>(VM_LANG_ZH));
  prefs.putInt(kNvsDateKey, cachedDateKey_);
  const size_t written = prefs.putString(kNvsQuoteKey, quote_);
  prefs.end();
  return written == quote_.length();
}

bool DailyQuoteClient::refreshIfNeeded(time_t nowEpoch, bool allowNetwork)
{
  const int today = dateKey(nowEpoch);
  if (today <= 0) {
    if (quote_.length() == 0) quote_ = fallbackQuote();
    return false;
  }
  if (cachedDateKey_ == today && quote_.length() > 0) return false;
  if (!allowNetwork) {
    if (quote_.length() == 0) quote_ = fallbackQuote();
    return false;
  }

  String fresh;
  if (!requestQuote(nowEpoch, fresh)) {
    if (quote_.length() == 0) quote_ = fallbackQuote();
    return false;
  }

  quote_ = fresh;
  cachedDateKey_ = today;
  save();
  return true;
}

bool DailyQuoteClient::needsRefresh(time_t nowEpoch) const
{
  const int today = dateKey(nowEpoch);
  return today > 0 && (cachedDateKey_ != today || quote_.length() == 0);
}

bool DailyQuoteClient::requestQuote(time_t nowEpoch, String& outQuote)
{
  if (cfg_.provider != VM_MEMO_OPENAI_COMPATIBLE) return false;

  String system;
  system.reserve(640);
#if VM_LANG_ZH
  system += "Return ONLY JSON: {\\\"quote\\\":\\\"...\\\"}. ";
  system += "The quote must be Simplified Chinese, one sentence, under 28 Chinese characters, ";
  system += "positive, encouraging, lightly funny, and suitable for a desk reminder. ";
  system += "No punctuation, no emoji, no markdown, no quotation marks inside the value. ";
  system += "Use a space instead of punctuation if a pause is needed.";
#else
  system += "Return ONLY JSON: {\\\"quote\\\":\\\"...\\\"}. ";
  system += "The quote must be English, one sentence, under 12 words, ";
  system += "positive, encouraging, lightly funny, and suitable for a desk reminder. ";
  system += "No emoji, no markdown, no quotation marks inside the value.";
#endif

  String user;
  user += "Local date: ";
  user += formatLocalDate(nowEpoch);
  user += ". Generate today's line.";

  String body;
  body.reserve(system.length() + user.length() + 256);
  body += "{\"model\":\"";
  body += voice_memo::jsonEscape(cfg_.model);
  body += "\",\"temperature\":0.8,";
  body += "\"response_format\":{\"type\":\"json_object\"},";
  body += "\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"";
  body += system;
  body += "\"},";
  body += "{\"role\":\"user\",\"content\":\"";
  body += voice_memo::jsonEscape(user);
  body += "\"}]}";

  HTTPClient http;
  WiFiClientSecure secure;
  http.setTimeout(httpTimeoutMs_);
  if (!beginHttp(http, secure, cfg_.url)) return false;

  http.addHeader("Authorization", String("Bearer ") + cfg_.apiKey);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(body);
  const String response = http.getString();
  http.end();
  Serial1.printf("[quote/openai] status=%d bytes=%u\n",
                 code, static_cast<unsigned>(response.length()));

  if (code != 200) return false;

  const String content = voice_memo::jsonStringValue(response, "content");
  const String obj = voice_memo::jsonExtractObject(content.length() ? content : response);
  const String quote = vmSanitizeDisplayText(trimQuote(voice_memo::jsonStringValue(obj, "quote")));
  if (quote.length() == 0) return false;

  outQuote = quote;
  return true;
}
