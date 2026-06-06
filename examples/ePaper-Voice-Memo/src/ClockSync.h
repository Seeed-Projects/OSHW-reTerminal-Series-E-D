#ifndef VOICE_MEMO_CLOCK_SYNC_H
#define VOICE_MEMO_CLOCK_SYNC_H

#include <time.h>

// Returns true when the RTC must be seeded from the firmware build time.
// 当 RTC 无效或明显早于固件构建时间时，返回 true。
inline bool vmShouldSyncRtcFromBuildTime(time_t rtcEpoch,
                                         time_t buildEpoch,
                                         time_t thresholdSeconds)
{
  if (buildEpoch <= 0) return false;
  if (rtcEpoch <= 0) return true;
  if (thresholdSeconds < 0) thresholdSeconds = 0;
  return (rtcEpoch + thresholdSeconds) < buildEpoch;
}

#endif  // VOICE_MEMO_CLOCK_SYNC_H
