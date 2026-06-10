#include <unity.h>
#include "MemoReplacePolicy.h"

void test_earliest_due_wins()
{
    const time_t due[] = {300, 100, 200};
    const bool hasDue[] = {true, true, true};
    TEST_ASSERT_EQUAL_UINT(1, vmFindEarliestDueIndex(due, hasDue, 3));
}

void test_no_due_entries_are_last_choice()
{
    const time_t due[] = {0, 200, 100};
    const bool hasDue[] = {false, true, true};
    TEST_ASSERT_EQUAL_UINT(2, vmFindEarliestDueIndex(due, hasDue, 3));
}

void test_all_no_due_falls_back_to_first()
{
    const time_t due[] = {0, 0, 0};
    const bool hasDue[] = {false, false, false};
    TEST_ASSERT_EQUAL_UINT(0, vmFindEarliestDueIndex(due, hasDue, 3));
}

void test_fifth_entry_replaces_visible_earliest_slot()
{
    int ids[] = {1, 2, 3, 4};
    const int newId = 5;
    const time_t due[] = {400, 100, 300, 200};
    const bool hasDue[] = {true, true, true, true};

    const size_t replaceIdx = vmFindEarliestDueIndex(due, hasDue, 4);
    ids[replaceIdx] = newId;

    TEST_ASSERT_EQUAL_UINT(1, replaceIdx);
    TEST_ASSERT_EQUAL_INT(newId, ids[replaceIdx]);
    TEST_ASSERT_EQUAL_INT(1, ids[0]);
    TEST_ASSERT_EQUAL_INT(3, ids[2]);
    TEST_ASSERT_EQUAL_INT(4, ids[3]);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_earliest_due_wins);
    RUN_TEST(test_no_due_entries_are_last_choice);
    RUN_TEST(test_all_no_due_falls_back_to_first);
    RUN_TEST(test_fifth_entry_replaces_visible_earliest_slot);
    return UNITY_END();
}
