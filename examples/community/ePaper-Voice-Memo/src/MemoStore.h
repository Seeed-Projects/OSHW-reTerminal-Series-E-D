// MemoStore.h -- in-memory reminder list with NVS persistence.
//
// Each reminder is one MemoEntry: a short English sentence, an optional
// short "fuzzy" label (used when the user spoke about time vaguely), an
// event-time epoch, and a "done" flag. The list survives power cycles
// because it is mirrored into the ESP32 non-volatile storage (NVS) under
// namespace "vmm", key "items".
//
// The list works like a fixed-size rolling window. E1003 can keep all kMax
// entries; smaller non-touch panels can use addWithinVisibleLimit() so the
// page replaces the earliest visible due item once the visible page is full.
//
// Persistence format (versioned so future changes stay forward compatible):
//
//   uint8  version (currently 2)
//   uint8  count
//   for i in 0..count:
//     int64  dueEpoch  (little-endian; 0 when hasDue is false)
//     uint8  flags     (bit0 = hasDue, bit1 = done)
//     uint16 textLen
//     uint8  text[textLen]
//     uint16 fuzzyLen
//     uint8  fuzzy[fuzzyLen]
//
// On NVS version mismatch the old blob is ignored (no migration), because
// this is a developer example and migration code is more trouble than it
// would save.

#ifndef VOICE_MEMO_STORE_H
#define VOICE_MEMO_STORE_H

#include <Arduino.h>
#include <time.h>

struct MemoEntry {
  String text;        // one English reminder sentence
  String fuzzyLabel;  // optional short label like "Tonight", "Tmrw AM"; "" = precise time
  time_t dueEpoch;    // event time, Unix epoch (local)
  bool   hasDue;
  bool   done;        // user tapped the checkbox
};

class MemoStore {
 public:
  static constexpr size_t kMax = 8;

  MemoStore();

  // Loads previously stored reminders from NVS. Safe to call once at boot.
  // Returns true if at least one entry was loaded.
  bool begin();

  // Appends an entry. If the list is full, first try to evict an entry
  // whose done flag is true (so completed work makes room for new work);
  // otherwise drop the oldest. Persists the list to NVS at the end.
  bool add(const MemoEntry& entry);

  // Appends inside the current one-page visible capacity. If that visible
  // page is already full, replace the entry with the earliest due time.
  // 在当前单页显示容量内追加；如果这一页已满，则覆盖提醒时间最早的条目。
  bool addWithinVisibleLimit(const MemoEntry& entry, size_t visibleMax);

  // Reorders the list in place so undone-upcoming come first (asc by due),
  // then undone-overdue (desc by due), then done items at the bottom (desc
  // by due). Does NOT touch NVS -- sort order changes per render.
  void sortByDue(time_t now);

  // Flips the done flag of the entry at visual index i (i.e. the index
  // after the last sortByDue call) and persists. Returns false on NVS
  // write failures (in-memory state is still updated).
  bool toggleDone(size_t i);

  // Removes every entry and clears NVS. Useful for "factory reset" hooks.
  bool clear();

  size_t           count() const { return count_; }
  const MemoEntry& at(size_t i) const { return items_[i]; }

 private:
  static constexpr uint8_t kBlobVersion = 2;
  // 8 entries * (8 + 1 + 2 + 200 + 2 + 32) ~= 2 KB. Reserve 4 KB to leave
  // headroom for longer transcripts a future contributor might want.
  static constexpr size_t kMaxBlobBytes = 4096;

  MemoEntry items_[kMax];
  size_t    count_;

  // Compares the NVS language tag against the firmware language and drops the
  // reminder blob when they differ. Called at the start of begin().
  void reconcileLanguage();

  bool load();
  bool save();
};

#endif  // VOICE_MEMO_STORE_H
