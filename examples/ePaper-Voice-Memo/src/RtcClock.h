// RtcClock.h -- PCF8563 real-time clock driver.
//
// The reTerminal E-series carries a PCF8563 on the I2C bus. This class owns
// the bus configuration and exposes only the few operations the app actually
// needs:
//
//   - begin(sda, scl): initialize the bus, probe the chip, and set the clock
//     to the firmware build time on the very first power-up so the device has
//     a sensible default before the user manually sets a real time.
//   - available(): true when the chip responded and its low-voltage flag is
//     clear, meaning the time on the chip is trustworthy.
//   - nowEpoch(): current local time as a Unix epoch. Used by MemoStore for
//     sorting and by MemoClient to give the LLM a "today" reference.
//   - nowTimeLabel() / nowDateLabel(): pre-formatted strings for the UI.
//
// Future contributors swapping to a different RTC chip should keep this
// public surface and only rewrite the .cpp.

#ifndef VOICE_MEMO_RTC_CLOCK_H
#define VOICE_MEMO_RTC_CLOCK_H

#include <Arduino.h>
#include <time.h>

struct VoiceMemoRtcTime {
  int year;
  int month;
  int day;
  int weekday;
  int hour;
  int minute;
  int second;
  bool voltageOK;
};

class RtcClock {
 public:
  RtcClock();

  bool begin(uint8_t sdaPin, uint8_t sclPin);
  bool available() const { return available_; }

  bool readTime(VoiceMemoRtcTime& out);
  time_t nowEpoch();
  String nowTimeLabel();
  String nowDateLabel();
  // Returns a long-form date string for the header, e.g. "Friday, May 30".
  String nowLongDateLabel();
  // Compact header date, e.g. "Apr 28 Tue".
  String nowHeaderDateLabel();

 private:
  static constexpr uint8_t kAddress = 0x51;
  static constexpr uint8_t kRegCtrl1   = 0x00;
  static constexpr uint8_t kRegCtrl2   = 0x01;
  static constexpr uint8_t kRegSeconds = 0x02;
  static constexpr uint8_t kRegClkout  = 0x0D;
  static constexpr time_t kBuildSyncThresholdSeconds = 60;

  bool readRegs(uint8_t reg, uint8_t* buf, size_t len);
  bool writeReg(uint8_t reg, uint8_t value);
  bool probe();
  bool chipInit();
  bool voltageOK();
  bool setTime(int year, int month, int day, int hour, int minute, int second);
  void seedFromBuildTime();

  bool available_;
};

#endif  // VOICE_MEMO_RTC_CLOCK_H
