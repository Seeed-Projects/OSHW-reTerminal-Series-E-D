#include <unity.h>
#include "ClockSync.h"

void test_invalid_rtc_syncs_from_build_time()
{
    TEST_ASSERT_TRUE(vmShouldSyncRtcFromBuildTime(0, 1780480800, 60));
}

void test_rtc_before_build_time_syncs()
{
    const time_t buildEpoch = 1780480800;
    const time_t rtcEpoch = buildEpoch - 120;
    TEST_ASSERT_TRUE(vmShouldSyncRtcFromBuildTime(rtcEpoch, buildEpoch, 60));
}

void test_rtc_within_threshold_does_not_sync()
{
    const time_t buildEpoch = 1780480800;
    const time_t rtcEpoch = buildEpoch - 30;
    TEST_ASSERT_FALSE(vmShouldSyncRtcFromBuildTime(rtcEpoch, buildEpoch, 60));
}

void test_rtc_after_build_time_does_not_rewind()
{
    const time_t buildEpoch = 1780480800;
    const time_t rtcEpoch = buildEpoch + 3600;
    TEST_ASSERT_FALSE(vmShouldSyncRtcFromBuildTime(rtcEpoch, buildEpoch, 60));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_invalid_rtc_syncs_from_build_time);
    RUN_TEST(test_rtc_before_build_time_syncs);
    RUN_TEST(test_rtc_within_threshold_does_not_sync);
    RUN_TEST(test_rtc_after_build_time_does_not_rewind);
    return UNITY_END();
}
