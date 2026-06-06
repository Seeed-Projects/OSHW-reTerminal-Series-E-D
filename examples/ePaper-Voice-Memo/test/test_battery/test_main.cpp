#include <unity.h>
#include "BatteryMath.h"

void test_full()    { TEST_ASSERT_EQUAL_INT(100, vmBatteryPercent(2100)); } // 2.1*2=4.2V
void test_empty()   { TEST_ASSERT_EQUAL_INT(0,   vmBatteryPercent(1650)); } // 1.65*2=3.3V
void test_half()    { TEST_ASSERT_EQUAL_INT(50,  vmBatteryPercent(1875)); } // 1.875*2=3.75V
void test_clamp_hi(){ TEST_ASSERT_EQUAL_INT(100, vmBatteryPercent(2300)); }
void test_clamp_lo(){ TEST_ASSERT_EQUAL_INT(0,   vmBatteryPercent(1000)); }

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_full);
    RUN_TEST(test_empty);
    RUN_TEST(test_half);
    RUN_TEST(test_clamp_hi);
    RUN_TEST(test_clamp_lo);
    return UNITY_END();
}
