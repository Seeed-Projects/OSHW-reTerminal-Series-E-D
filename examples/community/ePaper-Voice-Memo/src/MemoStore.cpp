#include "MemoStore.h"

#include <Preferences.h>

#include "DisplayText.h"
#include "MemoReplacePolicy.h"
#include "UiLang.h"

namespace {

constexpr const char* kNvsNamespace = "vmm";
constexpr const char* kNvsKey       = "items";
constexpr const char* kNvsLangKey   = "lang";

// Time used to sort entries with no due time. They sink to the very end of
// the upcoming section by pretending their due is far in the future.
constexpr time_t kNoDueSortKey = static_cast<time_t>(0x7FFFFFFFLL);

constexpr uint8_t kFlagHasDue = 0x01;
constexpr uint8_t kFlagDone   = 0x02;

void writeLE16(uint8_t* dst, uint16_t value)
{
  dst[0] = static_cast<uint8_t>(value);
  dst[1] = static_cast<uint8_t>(value >> 8);
}

uint16_t readLE16(const uint8_t* src)
{
  return static_cast<uint16_t>(src[0])
       | (static_cast<uint16_t>(src[1]) << 8);
}

void writeLE64(uint8_t* dst, int64_t value)
{
  for (int i = 0; i < 8; i++) {
    dst[i] = static_cast<uint8_t>(value >> (i * 8));
  }
}

int64_t readLE64(const uint8_t* src)
{
  int64_t v = 0;
  for (int i = 0; i < 8; i++) {
    v |= static_cast<int64_t>(src[i]) << (i * 8);
  }
  return v;
}

}  // namespace

MemoStore::MemoStore()
  : items_{},
    count_(0)
{
}

bool MemoStore::begin()
{
  reconcileLanguage();
  return load();
}

void MemoStore::reconcileLanguage()
{
  // Reminders are stored in one language. When the firmware language differs
  // from the tag saved beside the blob (or no tag exists yet), drop the blob
  // so the device starts clean in the current language. Opening read-write
  // creates the namespace on first boot, so the tag is always written once.
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, /*readOnly=*/false)) return;

  const uint8_t raw = prefs.getUChar(kNvsLangKey, 0xFF);
  const int storedTag = (raw == 0xFF) ? -1 : static_cast<int>(raw);
  if (vmShouldWipeForLanguage(storedTag, VM_LANG_ZH)) {
    prefs.remove(kNvsKey);
    prefs.putUChar(kNvsLangKey, static_cast<uint8_t>(VM_LANG_ZH));
  }
  prefs.end();
}

bool MemoStore::load()
{
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, /*readOnly=*/true)) {
    count_ = 0;
    return false;
  }

  const size_t blobLen = prefs.getBytesLength(kNvsKey);
  if (blobLen == 0 || blobLen > kMaxBlobBytes) {
    prefs.end();
    count_ = 0;
    return false;
  }

  uint8_t buf[kMaxBlobBytes];
  const size_t read = prefs.getBytes(kNvsKey, buf, blobLen);
  prefs.end();
  if (read != blobLen || read < 2) {
    count_ = 0;
    return false;
  }

  size_t offset = 0;
  const uint8_t version = buf[offset++];
  const uint8_t count   = buf[offset++];
  if (version != kBlobVersion || count > kMax) {
    // Different version -> ignore (no migration on purpose).
    count_ = 0;
    return false;
  }

  count_ = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (offset + 8 + 1 + 2 > read) { count_ = 0; return false; }
    const int64_t  due   = readLE64(buf + offset); offset += 8;
    const uint8_t  flags = buf[offset++];
    const uint16_t tlen  = readLE16(buf + offset); offset += 2;
    if (offset + tlen + 2 > read) { count_ = 0; return false; }

    MemoEntry& e = items_[count_++];
    e.dueEpoch = static_cast<time_t>(due);
    e.hasDue   = (flags & kFlagHasDue) != 0;
    e.done     = (flags & kFlagDone)   != 0;

    e.text = "";
    e.text.reserve(tlen);
    for (uint16_t j = 0; j < tlen; j++) {
      e.text += static_cast<char>(buf[offset + j]);
    }
    e.text = vmSanitizeDisplayText(e.text);
    offset += tlen;

    const uint16_t flen = readLE16(buf + offset); offset += 2;
    if (offset + flen > read) { count_ = 0; return false; }
    e.fuzzyLabel = "";
    e.fuzzyLabel.reserve(flen);
    for (uint16_t j = 0; j < flen; j++) {
      e.fuzzyLabel += static_cast<char>(buf[offset + j]);
    }
    e.fuzzyLabel = vmSanitizeDisplayText(e.fuzzyLabel);
    offset += flen;
  }
  return count_ > 0;
}

bool MemoStore::save()
{
  uint8_t buf[kMaxBlobBytes];
  size_t  offset = 0;

  buf[offset++] = kBlobVersion;
  buf[offset++] = static_cast<uint8_t>(count_);

  for (size_t i = 0; i < count_; i++) {
    const MemoEntry& e = items_[i];
    const uint16_t tlen = static_cast<uint16_t>(e.text.length());
    const uint16_t flen = static_cast<uint16_t>(e.fuzzyLabel.length());

    if (offset + 8 + 1 + 2 + tlen + 2 + flen > sizeof(buf)) return false;

    writeLE64(buf + offset, static_cast<int64_t>(e.dueEpoch)); offset += 8;
    uint8_t flags = 0;
    if (e.hasDue) flags |= kFlagHasDue;
    if (e.done)   flags |= kFlagDone;
    buf[offset++] = flags;

    writeLE16(buf + offset, tlen); offset += 2;
    memcpy(buf + offset, e.text.c_str(), tlen);
    offset += tlen;

    writeLE16(buf + offset, flen); offset += 2;
    memcpy(buf + offset, e.fuzzyLabel.c_str(), flen);
    offset += flen;
  }

  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, /*readOnly=*/false)) return false;
  const size_t written = prefs.putBytes(kNvsKey, buf, offset);
  prefs.end();
  return written == offset;
}

bool MemoStore::add(const MemoEntry& entry)
{
  MemoEntry clean = entry;
  clean.text = vmSanitizeDisplayText(clean.text);
  clean.fuzzyLabel = vmSanitizeDisplayText(clean.fuzzyLabel);

  if (count_ < kMax) {
    items_[count_++] = clean;
    return save();
  }

  // Full: prefer to evict the first done entry so completed work makes room
  // for new work. Fall back to dropping the oldest entry when nothing is done.
  int evictIdx = -1;
  for (size_t i = 0; i < kMax; i++) {
    if (items_[i].done) { evictIdx = static_cast<int>(i); break; }
  }
  if (evictIdx < 0) evictIdx = 0;

  for (size_t i = static_cast<size_t>(evictIdx); i + 1 < kMax; i++) {
    items_[i] = items_[i + 1];
  }
  items_[kMax - 1] = clean;
  return save();
}

bool MemoStore::addWithinVisibleLimit(const MemoEntry& entry, size_t visibleMax)
{
  MemoEntry clean = entry;
  clean.text = vmSanitizeDisplayText(clean.text);
  clean.fuzzyLabel = vmSanitizeDisplayText(clean.fuzzyLabel);

  if (visibleMax == 0) visibleMax = 1;
  if (visibleMax > kMax) visibleMax = kMax;

  if (count_ < visibleMax) {
    items_[count_++] = clean;
    return save();
  }

  const size_t windowCount = visibleMax;
  time_t dueEpochs[MemoStore::kMax] = {};
  bool hasDue[MemoStore::kMax] = {};
  for (size_t i = 0; i < windowCount; i++) {
    dueEpochs[i] = items_[i].dueEpoch;
    hasDue[i] = items_[i].hasDue;
  }

  const size_t replaceIdx =
      vmFindEarliestDueIndex(dueEpochs, hasDue, windowCount);
  items_[replaceIdx] = clean;
  count_ = visibleMax;
  return save();
}

void MemoStore::sortByDue(time_t now)
{
  // Three buckets in display order:
  //   0 = undone upcoming  (asc by due, soonest first)
  //   1 = undone overdue   (desc by due, most-recently overdue first)
  //   2 = done             (desc by due)
  auto bucket = [&](const MemoEntry& e) -> int {
    if (e.done) return 2;
    if (e.hasDue && e.dueEpoch < now) return 1;
    return 0;
  };
  auto sortKey = [&](const MemoEntry& e) -> int64_t {
    if (!e.hasDue) return static_cast<int64_t>(kNoDueSortKey);
    return static_cast<int64_t>(e.dueEpoch);
  };
  auto earlier = [&](const MemoEntry& a, const MemoEntry& b) -> bool {
    const int ba = bucket(a);
    const int bb = bucket(b);
    if (ba != bb) return ba < bb;
    const int64_t ka = sortKey(a);
    const int64_t kb = sortKey(b);
    if (ba == 0) return ka < kb;   // upcoming: soonest first
    return ka > kb;                // overdue / done: most recent first
  };

  // Insertion sort (n <= 8): tiny binary, no <algorithm> include.
  for (size_t i = 1; i < count_; i++) {
    MemoEntry key = items_[i];
    size_t j = i;
    while (j > 0 && earlier(key, items_[j - 1])) {
      items_[j] = items_[j - 1];
      j--;
    }
    items_[j] = key;
  }
}

bool MemoStore::toggleDone(size_t i)
{
  if (i >= count_) return false;
  items_[i].done = !items_[i].done;
  return save();
}

bool MemoStore::clear()
{
  count_ = 0;
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, /*readOnly=*/false)) return false;
  prefs.remove(kNvsKey);
  prefs.end();
  return true;
}
