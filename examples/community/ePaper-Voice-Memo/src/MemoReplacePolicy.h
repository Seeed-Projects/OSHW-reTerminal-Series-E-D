#ifndef VOICE_MEMO_REPLACE_POLICY_H
#define VOICE_MEMO_REPLACE_POLICY_H

#include <stddef.h>
#include <time.h>

// Returns the index with the earliest due time. Entries without due times are
// only selected when every visible entry has no due time.
// 返回提醒时间最早的索引。没有提醒时间的条目只会在整页都没有时间时被选中。
static inline size_t vmFindEarliestDueIndex(const time_t* dueEpochs,
                                            const bool* hasDue,
                                            size_t count)
{
  if (count == 0) return 0;

  size_t best = 0;
  bool foundDue = false;
  time_t bestDue = 0;

  for (size_t i = 0; i < count; i++) {
    if (!hasDue[i]) continue;
    if (!foundDue || dueEpochs[i] < bestDue) {
      best = i;
      bestDue = dueEpochs[i];
      foundDue = true;
    }
  }

  return foundDue ? best : 0;
}

#endif  // VOICE_MEMO_REPLACE_POLICY_H
