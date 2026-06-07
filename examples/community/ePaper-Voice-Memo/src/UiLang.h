// UiLang.h -- compile-time UI language selection and the fixed-string table.
//
// The whole firmware is built for exactly one language, chosen at compile time
// by the build flag VM_UI_LANG_ZH (set in the Chinese platformio env). This
// header is the single source of truth both for that decision (VM_LANG_ZH) and
// for every fixed, non-user-generated UI word.
//
// Each word is exposed as an English / Chinese pair (uiStrEn / uiStrZh) with a
// macro picking the active column (uiStr). The pair shape lets a native unit
// test assert both languages from one build, mirroring DateLabels.h.

#ifndef VOICE_MEMO_UI_LANG_H
#define VOICE_MEMO_UI_LANG_H

// 1 for the Chinese build, 0 for the English build.
#if defined(VM_UI_LANG_ZH)
  #define VM_LANG_ZH 1
#else
  #define VM_LANG_ZH 0
#endif

// One id per fixed UI string. kCount is a sentinel for iteration in tests.
enum class UiStringId {
  kAppName,        // header logo word
  kHintAdd,        // main idle hint
  kHintTooShort,   // recording shorter than the minimum
  kHintNoWifi,     // reminder dropped because WiFi is down
  kHintMaxLen,     // recording hit the maximum length
  kEmptyList,      // empty reminder list placeholder
  kProcessing,     // header "processing" tag
  kReminders,      // list title / fallback badge
  kBootStarting,   // boot splash: starting
  kBootWifi,       // boot splash: connecting WiFi
  kSomeDay,        // due label when the entry has no due time
  kOverdue,        // due label when the entry is past due
  kNoSpeech,       // placeholder memo when speech was not recognized
  kCount
};

// English column.
inline const char* uiStrEn(UiStringId id) {
  switch (id) {
    case UiStringId::kAppName:      return "Voice Memo";
    case UiStringId::kHintAdd:      return "Hold KEY0 to add. Tap a box to check off.";
    case UiStringId::kHintTooShort: return "Hold KEY0 for at least one second.";
    case UiStringId::kHintNoWifi:   return "Reminder skipped because WiFi is unavailable.";
    case UiStringId::kHintMaxLen:   return "Stopped at max length. Hold KEY0 for another memo.";
    case UiStringId::kEmptyList:    return "Hold KEY0 and speak to add your first reminder.";
    case UiStringId::kProcessing:   return "Processing";
    case UiStringId::kReminders:    return "Reminders";
    case UiStringId::kBootStarting: return "Starting...";
    case UiStringId::kBootWifi:     return "Connecting WiFi...";
    case UiStringId::kSomeDay:      return "Some day";
    case UiStringId::kOverdue:      return "Overdue";
    case UiStringId::kNoSpeech:     return "No speech recognized.";
    default:                        return "";
  }
}

// Chinese column.
inline const char* uiStrZh(UiStringId id) {
  switch (id) {
    case UiStringId::kAppName:      return "语音备忘录";
    case UiStringId::kHintAdd:      return "长按 KEY0 添加  点方框勾选完成";
    case UiStringId::kHintTooShort: return "请长按 KEY0 至少一秒";
    case UiStringId::kHintNoWifi:   return "WiFi 不可用 本次提醒未保存";
    case UiStringId::kHintMaxLen:   return "已到最长录音 再次长按 KEY0 继续";
    case UiStringId::kEmptyList:    return "长按 KEY0 说话 添加第一条提醒";
    case UiStringId::kProcessing:   return "处理中";
    case UiStringId::kReminders:    return "提醒";
    case UiStringId::kBootStarting: return "启动中";
    case UiStringId::kBootWifi:     return "连接 WiFi";
    case UiStringId::kSomeDay:      return "某天";
    case UiStringId::kOverdue:      return "已逾期";
    case UiStringId::kNoSpeech:     return "未识别到语音";
    default:                        return "";
  }
}

// Active column for the current build.
#if VM_LANG_ZH
inline const char* uiStr(UiStringId id) { return uiStrZh(id); }
#else
inline const char* uiStr(UiStringId id) { return uiStrEn(id); }
#endif

// True when a stored language tag requires wiping the reminder store: the tag
// is missing (-1) or differs from the firmware's language. Pure for testing.
inline bool vmShouldWipeForLanguage(int storedTag, int firmwareTag) {
  return storedTag != firmwareTag;
}

#endif  // VOICE_MEMO_UI_LANG_H
