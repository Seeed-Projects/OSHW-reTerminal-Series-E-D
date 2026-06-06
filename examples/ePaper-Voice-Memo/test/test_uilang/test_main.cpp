#include <unity.h>
#include <string.h>
#include "DisplayText.h"
#include "UiLang.h"

// The string table is exposed as two columns (uiStrEn / uiStrZh) so this one
// native build can assert both languages regardless of VM_LANG_ZH.

void test_en_column_spot_values() {
    TEST_ASSERT_EQUAL_STRING("Voice Memo", uiStrEn(UiStringId::kAppName));
    TEST_ASSERT_EQUAL_STRING("Processing", uiStrEn(UiStringId::kProcessing));
    TEST_ASSERT_EQUAL_STRING("Reminders",  uiStrEn(UiStringId::kReminders));
    TEST_ASSERT_EQUAL_STRING("Overdue",    uiStrEn(UiStringId::kOverdue));
}

void test_zh_column_spot_values() {
    TEST_ASSERT_EQUAL_STRING("语音备忘录", uiStrZh(UiStringId::kAppName));
    TEST_ASSERT_EQUAL_STRING("处理中", uiStrZh(UiStringId::kProcessing));
    TEST_ASSERT_EQUAL_STRING("提醒",   uiStrZh(UiStringId::kReminders));
    TEST_ASSERT_EQUAL_STRING("已逾期", uiStrZh(UiStringId::kOverdue));
}

void test_zh_hint_add_uses_spaces_instead_of_punctuation() {
    const char* hint = uiStrZh(UiStringId::kHintAdd);
    TEST_ASSERT_EQUAL_STRING("长按 KEY0 添加  点方框勾选完成", hint);
    TEST_ASSERT_NOT_NULL(strstr(hint, "  "));
    TEST_ASSERT_NULL(strstr(hint, "，"));
    TEST_ASSERT_NULL(strstr(hint, "。"));
}

void test_zh_display_text_replaces_punctuation_with_spaces() {
    const std::string text = vmSanitizeDisplayTextForLang("买苹果，香蕉。OK! \"Go?\"", true);
    TEST_ASSERT_EQUAL_STRING("买苹果 香蕉 OK Go", text.c_str());
}

void test_en_display_text_keeps_punctuation() {
    const std::string text = vmSanitizeDisplayTextForLang("Buy apples, then go.", false);
    TEST_ASSERT_EQUAL_STRING("Buy apples, then go.", text.c_str());
}

// Every id must resolve to a non-empty string in both columns, and the two
// columns must differ (catches a copy-paste that left a cell in English).
void test_every_id_nonempty_and_distinct() {
    for (int i = 0; i < static_cast<int>(UiStringId::kCount); i++) {
        const UiStringId id = static_cast<UiStringId>(i);
        TEST_ASSERT_TRUE(uiStrEn(id)[0] != '\0');
        TEST_ASSERT_TRUE(uiStrZh(id)[0] != '\0');
        TEST_ASSERT_TRUE(strcmp(uiStrEn(id), uiStrZh(id)) != 0);
    }
}

void test_wipe_when_tag_differs_or_missing() {
    // Same language -> keep.
    TEST_ASSERT_FALSE(vmShouldWipeForLanguage(0, 0));
    TEST_ASSERT_FALSE(vmShouldWipeForLanguage(1, 1));
    // Different language -> wipe.
    TEST_ASSERT_TRUE(vmShouldWipeForLanguage(0, 1));
    TEST_ASSERT_TRUE(vmShouldWipeForLanguage(1, 0));
    // Missing tag (-1) -> wipe for either firmware language.
    TEST_ASSERT_TRUE(vmShouldWipeForLanguage(-1, 0));
    TEST_ASSERT_TRUE(vmShouldWipeForLanguage(-1, 1));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_en_column_spot_values);
    RUN_TEST(test_zh_column_spot_values);
    RUN_TEST(test_zh_hint_add_uses_spaces_instead_of_punctuation);
    RUN_TEST(test_zh_display_text_replaces_punctuation_with_spaces);
    RUN_TEST(test_en_display_text_keeps_punctuation);
    RUN_TEST(test_every_id_nonempty_and_distinct);
    RUN_TEST(test_wipe_when_tag_differs_or_missing);
    return UNITY_END();
}
