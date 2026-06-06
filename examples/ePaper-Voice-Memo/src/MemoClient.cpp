#include "MemoClient.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>

#include "JsonUtil.h"
#include "DisplayText.h"
#include "UiLang.h"

namespace {

constexpr time_t kSanityFloorEpoch = 1577836800LL;  // 2020-01-01 00:00 UTC

bool beginHttp(HTTPClient& http, WiFiClientSecure& secure, const char* url)
{
  const String target(url);
  if (target.startsWith("https://")) {
    secure.setInsecure();
    return http.begin(secure, target);
  }
  return http.begin(target);
}

String formatLocalDateTime(time_t epoch)
{
  if (epoch <= 0) return "1970-01-01 00:00";
  struct tm t = {};
  localtime_r(&epoch, &t);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
  return String(buf);
}

const char* weekdayName(time_t epoch)
{
  static const char* names[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                "Thursday", "Friday", "Saturday"};
  struct tm t = {};
  localtime_r(&epoch, &t);
  return names[(t.tm_wday >= 0 && t.tm_wday < 7) ? t.tm_wday : 0];
}

}  // namespace

MemoClient::MemoClient()
  : cfg_{},
    httpTimeoutMs_(45000)
{
}

void MemoClient::configure(const MemoConfig& cfg, uint32_t httpTimeoutMs)
{
  cfg_ = cfg;
  httpTimeoutMs_ = httpTimeoutMs;
}

time_t MemoClient::parseLocalDateTime(const String& text)
{
  if (text.length() < 16) return 0;

  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  // Accept both "YYYY-MM-DD HH:MM" and "YYYY-MM-DDTHH:MM[:SS]".
  // sscanf is forgiving about the separator between date and time when we
  // use a literal ' ' in the format string, so try both forms in order.
  int scanned = sscanf(text.c_str(), "%d-%d-%d %d:%d:%d",
                       &year, &month, &day, &hour, &minute, &second);
  if (scanned < 5) {
    scanned = sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d",
                     &year, &month, &day, &hour, &minute, &second);
  }
  if (scanned < 5) return 0;

  if (year < 2020 || year > 2099) return 0;
  if (month < 1 || month > 12) return 0;
  if (day < 1 || day > 31) return 0;
  if (hour < 0 || hour > 23) return 0;
  if (minute < 0 || minute > 59) return 0;

  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon  = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min  = minute;
  t.tm_sec  = (scanned == 6) ? second : 0;
  t.tm_isdst = -1;
  const time_t epoch = mktime(&t);
  if (epoch < kSanityFloorEpoch) return 0;
  return epoch;
}

MemoEntry MemoClient::buildFallback(const String& transcript, time_t nowEpoch)
{
  MemoEntry e = summarizeWithRule(transcript, nowEpoch);
  // 1 hour from now keeps the entry sortable and visually meaningful. If RTC
  // is not available (nowEpoch == 0), leave hasDue false so the UI shows "--".
  if (nowEpoch > 0) {
    e.dueEpoch = nowEpoch + 3600;
    e.hasDue = true;
  } else {
    e.dueEpoch = 0;
    e.hasDue = false;
  }
  return e;
}

MemoEntry MemoClient::summarizeWithRule(const String& transcript, time_t nowEpoch)
{
  MemoEntry e;
  e.dueEpoch   = (nowEpoch > 0) ? (nowEpoch + 3600) : 0;
  e.hasDue     = nowEpoch > 0;
  e.done       = false;
  e.fuzzyLabel = "";   // rule-based fallback never produces a fuzzy label

  String text = transcript;
  text.trim();
  if (text.length() == 0) {
    e.text = "No speech recognized.";
    return e;
  }

  String lower = text;
  lower.toLowerCase();
  const char* prefixes[] = {
    "remind me to ",
    "please remind me to ",
    "remember to ",
    "note that ",
  };
  for (const char* prefix : prefixes) {
    const size_t n = strlen(prefix);
    if (lower.startsWith(prefix)) {
      text = text.substring(n);
      text.trim();
      break;
    }
  }
  if (text.length() > 0) {
    text.setCharAt(0, static_cast<char>(toupper(text[0])));
  }
  const char last = text.length() ? text[text.length() - 1] : '.';
  if (last != '.' && last != '!' && last != '?') text += '.';

  e.text = vmSanitizeDisplayText(text);
  return e;
}

MemoEntry MemoClient::summarizeOpenAICompatible(const String& transcript,
                                                time_t nowEpoch)
{
  String nowStr = formatLocalDateTime(nowEpoch);
  const char* weekday = (nowEpoch > 0) ? weekdayName(nowEpoch) : "Unknown";

  // The system prompt is INVERTED compared to a naive design: due_label is
  // assumed REQUIRED, and only becomes empty when the user said an exact
  // clock time. Weaker 8B-class models otherwise tend to "be helpful" and
  // guess a concrete time for phrases like "tonight" (defaulting to 8pm),
  // silently skipping the label and breaking the UX.
  //
  String system;
  system.reserve(2400);
#if VM_LANG_ZH
  // Chinese build: ask for a Chinese memo and Chinese time-of-day labels.
  // Same pre-escaped style as the English branch (\\n is a JSON newline,
  // \\\" a JSON quote) so it can be appended to the body unchanged.
  system += "你是一个嵌入式提醒设备的备忘提取引擎。";
  system += "用户的语音转写可能是中文、英文或中英混合。\\n\\n";
  system += "当前本地时间: ";
  system += nowStr;
  system += " (";
  system += weekday;
  system += ").\\n\\n";
  system += "只返回下面这个 JSON 对象, 不要任何解释, 不要 markdown 代码块。\\n";
  system += "{\\\"memo\\\":\\\"<简短中文提醒>\\\",\\\"due\\\":\\\"YYYY-MM-DD HH:MM\\\",\\\"due_label\\\":\\\"<时段词或留空>\\\"}\\n\\n";
  system += "字段规则:\\n";
  system += "1) memo: 一句简洁的中文提醒, 去掉口头语, 不要以\\\"记得\\\"开头。\\n";
  system += "   memo 中不要使用任何标点符号; 需要停顿时用空格。\\n";
  system += "2) due: 始终给出一个绝对的本地日期时间, 基于当前本地时间做最合理推断。\\n";
  system += "3) due_label: 只用来区分一天中的时段。\\n";
  system += "   - 已给出具体钟点(如 8、14:30、3点) -> due_label = \\\"\\\" (设备显示 HH:MM)。\\n";
  system += "   - 只说了某个时段、没有数字 -> 用一个词: 早上 / 下午 / 中午 / 晚上。\\n";
  system += "       早上/上午/morning -> 早上\\n";
  system += "       下午/afternoon -> 下午\\n";
  system += "       中午/noon -> 中午\\n";
  system += "       晚上/夜里/今晚/tonight/evening -> 晚上\\n";
  system += "   - 完全没有提到时间 -> due_label = \\\"NONE\\\" (设备不显示钟点)。\\n";
  system += "   日期由设备根据 due 计算。绝不要把日期词(今天/明天/本周)放进 due_label。\\n\\n";
  system += "示例:\\n";
  system += "输入: \\\"明天去跳舞\\\"\\n";
  system += "输出: {\\\"memo\\\":\\\"去跳舞\\\",\\\"due\\\":\\\"2026-05-31 09:00\\\",\\\"due_label\\\":\\\"NONE\\\"}\\n";
  system += "输入: \\\"提醒我今天晚上去洗澡\\\"\\n";
  system += "输出: {\\\"memo\\\":\\\"洗澡\\\",\\\"due\\\":\\\"2026-05-30 21:00\\\",\\\"due_label\\\":\\\"晚上\\\"}\\n";
  system += "输入: \\\"晚上8点叫我吃饭\\\"\\n";
  system += "输出: {\\\"memo\\\":\\\"吃饭\\\",\\\"due\\\":\\\"2026-05-30 20:00\\\",\\\"due_label\\\":\\\"\\\"}\\n";
  system += "输入: \\\"明天上午开会\\\"\\n";
  system += "输出: {\\\"memo\\\":\\\"开会\\\",\\\"due\\\":\\\"2026-05-31 09:00\\\",\\\"due_label\\\":\\\"早上\\\"}\\n";
  system += "输入: \\\"下午3点取快递\\\"\\n";
  system += "输出: {\\\"memo\\\":\\\"取快递\\\",\\\"due\\\":\\\"2026-05-30 15:00\\\",\\\"due_label\\\":\\\"\\\"}\\n";
  system += "输入: \\\"买点苹果\\\"\\n";
  system += "输出: {\\\"memo\\\":\\\"买苹果\\\",\\\"due\\\":\\\"2026-05-30 19:00\\\",\\\"due_label\\\":\\\"NONE\\\"}";
#else
  system += "You are a memo extraction engine for an embedded reminder ";
  system += "device. The user transcript may be Chinese, English, or mixed.\\n\\n";
  system += "CURRENT LOCAL TIME: ";
  system += nowStr;
  system += " (";
  system += weekday;
  system += ").\\n\\n";
  system += "Return ONLY this JSON object. No prose. No markdown fences.\\n";
  system += "{\\\"memo\\\":\\\"<short English reminder>\\\",\\\"due\\\":\\\"YYYY-MM-DD HH:MM\\\",\\\"due_label\\\":\\\"<time-of-day label or empty>\\\"}\\n\\n";
  system += "FIELD RULES:\\n";
  system += "1) memo: one concise English reminder sentence. Remove filler. ";
  system += "Do not start with 'Remember to'.\\n";
  system += "2) due: ALWAYS provide an absolute local datetime. Best-guess against CURRENT LOCAL TIME.\\n";
  system += "3) due_label: classifies the TIME-OF-DAY only.\\n";
  system += "   - Exact clock number given (8, 14:30, 3\\u70b9) -> due_label = \\\"\\\" (device shows HH:MM).\\n";
  system += "   - Only a part of day, no number -> use a FULL word: Morning / Afternoon / Noon / Evening.\\n";
  system += "       morning/\\u65e9\\u4e0a/\\u4e0a\\u5348 -> Morning\\n";
  system += "       afternoon/\\u4e0b\\u5348 -> Afternoon\\n";
  system += "       noon/\\u4e2d\\u5348 -> Noon\\n";
  system += "       evening/night/\\u665a\\u4e0a/tonight -> Evening\\n";
  system += "   - NO time mentioned at all -> due_label = \\\"NONE\\\" (device shows no clock).\\n";
  system += "   The DAY is computed by the device from `due`. NEVER put a day word (Today/Tomorrow/This wk) in due_label.\\n\\n";
  system += "EXAMPLES:\\n";
  system += "Input: \\\"\\u660e\\u5929\\u53bb\\u8df3\\u821e\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Go dancing\\\",\\\"due\\\":\\\"2026-05-31 09:00\\\",\\\"due_label\\\":\\\"NONE\\\"}\\n";
  system += "Input: \\\"\\u63d0\\u9192\\u6211\\u4eca\\u5929\\u665a\\u4e0a\\u53bb\\u6d17\\u6fa1\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Take a bath\\\",\\\"due\\\":\\\"2026-05-30 21:00\\\",\\\"due_label\\\":\\\"Evening\\\"}\\n";
  system += "Input: \\\"\\u665a\\u4e0a8\\u70b9\\u53eb\\u6211\\u5403\\u996d\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Dinner\\\",\\\"due\\\":\\\"2026-05-30 20:00\\\",\\\"due_label\\\":\\\"\\\"}\\n";
  system += "Input: \\\"\\u660e\\u5929\\u4e0a\\u5348\\u5f00\\u4f1a\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Morning meeting\\\",\\\"due\\\":\\\"2026-05-31 09:00\\\",\\\"due_label\\\":\\\"Morning\\\"}\\n";
  system += "Input: \\\"\\u4e0b\\u53483\\u70b9\\u53d6\\u5feb\\u9012\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Pick up package\\\",\\\"due\\\":\\\"2026-05-30 15:00\\\",\\\"due_label\\\":\\\"\\\"}\\n";
  system += "Input: \\\"\\u4e70\\u70b9\\u82f9\\u679c\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Buy apples\\\",\\\"due\\\":\\\"2026-05-30 19:00\\\",\\\"due_label\\\":\\\"NONE\\\"}";
#endif

  String body;
  body.reserve(transcript.length() + system.length() + 512);
  body += "{\"model\":\"";
  body += voice_memo::jsonEscape(cfg_.model);
  body += "\",\"temperature\":0,";
  body += "\"response_format\":{\"type\":\"json_object\"},";
  body += "\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"";
  body += system;
  body += "\"},";
  body += "{\"role\":\"user\",\"content\":\"";
  body += voice_memo::jsonEscape(transcript);
  body += "\"}]}";

  HTTPClient http;
  WiFiClientSecure secure;
  http.setTimeout(httpTimeoutMs_);
  if (!beginHttp(http, secure, cfg_.url)) return buildFallback(transcript, nowEpoch);

  http.addHeader("Authorization", String("Bearer ") + cfg_.apiKey);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(body);
  const String response = http.getString();
  http.end();
  Serial1.printf("[memo/openai] status=%d bytes=%u\n",
                 code, static_cast<unsigned>(response.length()));

  if (code != 200) return buildFallback(transcript, nowEpoch);

  // The chat response wraps our JSON in choices[0].message.content. Pull the
  // content string first, then extract our own object out of it.
  const String content = voice_memo::jsonStringValue(response, "content");
  if (content.length() == 0) return buildFallback(transcript, nowEpoch);

  const String obj = voice_memo::jsonExtractObject(content);
  if (obj.length() == 0) return buildFallback(transcript, nowEpoch);

  const String memo = voice_memo::jsonStringValue(obj, "memo");
  if (memo.length() == 0) return buildFallback(transcript, nowEpoch);

  MemoEntry e;
  e.text       = vmSanitizeDisplayText(memo);
  e.done       = false;
  e.fuzzyLabel = vmSanitizeDisplayText(voice_memo::jsonStringValue(obj, "due_label"));
  // Defense in depth: cap the fuzzy label so a chatty model cannot break
  // the right-side layout. The prompt asks for max 10 chars already.
  if (e.fuzzyLabel.length() > 12) {
    e.fuzzyLabel = e.fuzzyLabel.substring(0, 12);
  }
  const String due = voice_memo::jsonStringValue(obj, "due");
  const time_t parsed = parseLocalDateTime(due);
  if (parsed > 0) {
    e.dueEpoch = parsed;
    e.hasDue = true;
  } else if (nowEpoch > 0) {
    e.dueEpoch = nowEpoch + 3600;
    e.hasDue = true;
  } else {
    e.dueEpoch = 0;
    e.hasDue = false;
  }
  return e;
}

MemoEntry MemoClient::summarize(const String& transcript, time_t nowEpoch)
{
  if (cfg_.provider == VM_MEMO_OPENAI_COMPATIBLE) {
    return summarizeOpenAICompatible(transcript, nowEpoch);
  }
  return summarizeWithRule(transcript, nowEpoch);
}
