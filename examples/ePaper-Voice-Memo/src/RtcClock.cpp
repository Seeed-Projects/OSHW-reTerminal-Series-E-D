#include "RtcClock.h"

#include <Wire.h>

#include "ClockSync.h"
#include "UiLang.h"

namespace {

#if VM_LANG_ZH
const char* kWeekdayNames[] = {
  "周日", "周一", "周二", "周三", "周四", "周五", "周六"
};
#else
const char* kWeekdayNames[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
#endif

uint8_t bcdToDec(uint8_t bcd)
{
  return static_cast<uint8_t>(((bcd >> 4) * 10U) + (bcd & 0x0FU));
}

uint8_t decToBcd(uint8_t dec)
{
  return static_cast<uint8_t>(((dec / 10U) << 4) | (dec % 10U));
}

// Parses the __DATE__ and __TIME__ macros so the firmware can seed the RTC the
// first time it ever powers up with a fresh battery. This keeps the UI from
// showing "----" before the user manually sets a real time.
void getCompileTime(int& year, int& month, int& day,
                    int& hour, int& minute, int& second)
{
  const char* abbr = __DATE__;
  const char* names = "JanFebMarAprMayJunJulAugSepOctNovDec";
  month = 1;
  for (int i = 0; i < 12; i++) {
    if (strncmp(abbr, names + i * 3, 3) == 0) {
      month = i + 1;
      break;
    }
  }
  day = atoi(__DATE__ + 4);
  year = atoi(__DATE__ + 7);
  hour = atoi(__TIME__);
  minute = atoi(__TIME__ + 3);
  second = atoi(__TIME__ + 6);
}

time_t makeLocalEpoch(int year, int month, int day,
                      int hour, int minute, int second)
{
  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon  = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min  = minute;
  t.tm_sec  = second;
  t.tm_isdst = -1;
  return mktime(&t);
}

time_t rtcTimeToEpoch(const VoiceMemoRtcTime& rt)
{
  if (!rt.voltageOK) return 0;
  if (rt.year < 2000 || rt.year > 2099) return 0;
  if (rt.month < 1 || rt.month > 12) return 0;
  if (rt.day < 1 || rt.day > 31) return 0;
  if (rt.hour < 0 || rt.hour > 23) return 0;
  if (rt.minute < 0 || rt.minute > 59) return 0;
  if (rt.second < 0 || rt.second > 59) return 0;
  return makeLocalEpoch(rt.year, rt.month, rt.day,
                        rt.hour, rt.minute, rt.second);
}

time_t getBuildEpoch()
{
  int year, month, day, hour, minute, second;
  getCompileTime(year, month, day, hour, minute, second);
  return makeLocalEpoch(year, month, day, hour, minute, second);
}

}  // namespace

RtcClock::RtcClock()
  : available_(false)
{
}

bool RtcClock::readRegs(uint8_t reg, uint8_t* buf, size_t len)
{
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  const uint8_t received = Wire.requestFrom(static_cast<uint8_t>(kAddress),
                                            static_cast<uint8_t>(len));
  if (received != len) return false;

  for (size_t i = 0; i < len; i++) {
    buf[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool RtcClock::writeReg(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool RtcClock::probe()
{
  Wire.beginTransmission(kAddress);
  return Wire.endTransmission() == 0;
}

bool RtcClock::chipInit()
{
  if (!writeReg(kRegCtrl1, 0x00)) return false;
  if (!writeReg(kRegCtrl2, 0x00)) return false;
  if (!writeReg(kRegClkout, 0x00)) return false;
  return true;
}

bool RtcClock::voltageOK()
{
  uint8_t sec = 0;
  if (!readRegs(kRegSeconds, &sec, 1)) return false;
  return (sec & 0x80U) == 0U;
}

bool RtcClock::setTime(int year, int month, int day,
                       int hour, int minute, int second)
{
  if (year < 2000 || year > 2099) return false;
  if (month < 1 || month > 12) return false;
  if (day < 1 || day > 31) return false;
  if (hour < 0 || hour > 23) return false;
  if (minute < 0 || minute > 59) return false;
  if (second < 0 || second > 59) return false;

  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  mktime(&t);  // fills tm_wday for us.

  Wire.beginTransmission(kAddress);
  Wire.write(kRegSeconds);
  Wire.write(decToBcd(static_cast<uint8_t>(second)));
  Wire.write(decToBcd(static_cast<uint8_t>(minute)));
  Wire.write(decToBcd(static_cast<uint8_t>(hour)));
  Wire.write(decToBcd(static_cast<uint8_t>(day)));
  Wire.write(static_cast<uint8_t>(t.tm_wday));
  Wire.write(decToBcd(static_cast<uint8_t>(month)));
  Wire.write(decToBcd(static_cast<uint8_t>(year % 100)));
  return Wire.endTransmission() == 0;
}

bool RtcClock::readTime(VoiceMemoRtcTime& rt)
{
  uint8_t raw[7] = {};
  if (!readRegs(kRegSeconds, raw, 7)) return false;

  rt.voltageOK = (raw[0] & 0x80U) == 0U;
  rt.second  = bcdToDec(raw[0] & 0x7FU);
  rt.minute  = bcdToDec(raw[1] & 0x7FU);
  rt.hour    = bcdToDec(raw[2] & 0x3FU);
  rt.day     = bcdToDec(raw[3] & 0x3FU);
  rt.weekday = bcdToDec(raw[4] & 0x07U);
  rt.month   = bcdToDec(raw[5] & 0x1FU);

  const int yr = bcdToDec(raw[6]);
  rt.year = ((raw[5] & 0x80U) != 0U) ? (1900 + yr) : (2000 + yr);
  return true;
}

void RtcClock::seedFromBuildTime()
{
  int year, month, day, hour, minute, second;
  getCompileTime(year, month, day, hour, minute, second);
  setTime(year, month, day, hour, minute, second);
}

bool RtcClock::begin(uint8_t sdaPin, uint8_t sclPin)
{
  Wire.begin(sdaPin, sclPin);
  Wire.setClock(400000UL);

  if (!probe()) {
    available_ = false;
    return false;
  }
  if (!chipInit()) {
    available_ = false;
    return false;
  }

  VoiceMemoRtcTime rt = {};
  if (!readTime(rt)) {
    available_ = false;
    return false;
  }

  const time_t rtcEpoch = rtcTimeToEpoch(rt);
  const time_t buildEpoch = getBuildEpoch();
  if (vmShouldSyncRtcFromBuildTime(rtcEpoch, buildEpoch,
                                   kBuildSyncThresholdSeconds)) {
    seedFromBuildTime();
    if (readTime(rt) && rtcTimeToEpoch(rt) > 0) {
      Serial1.println("[rtc] seeded from firmware build time");
    }
  }

  available_ = rtcTimeToEpoch(rt) > 0;
  return available_;
}

time_t RtcClock::nowEpoch()
{
  VoiceMemoRtcTime rt = {};
  if (!available_ || !readTime(rt)) return 0;
  return rtcTimeToEpoch(rt);
}

String RtcClock::nowTimeLabel()
{
  VoiceMemoRtcTime rt = {};
  if (!available_ || !readTime(rt) || !rt.voltageOK) return "--:--";

  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", rt.hour, rt.minute);
  return String(buf);
}

String RtcClock::nowDateLabel()
{
  VoiceMemoRtcTime rt = {};
  if (!available_ || !readTime(rt) || !rt.voltageOK) return "--/-- ---";

  char buf[24];
  const int wd = (rt.weekday >= 0 && rt.weekday < 7) ? rt.weekday : 0;
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d %s",
           rt.year, rt.month, rt.day, kWeekdayNames[wd]);
  return String(buf);
}

String RtcClock::nowLongDateLabel()
{
  VoiceMemoRtcTime rt = {};
  if (!available_ || !readTime(rt) || !rt.voltageOK) return "---";

  const int wd = (rt.weekday >= 0 && rt.weekday < 7) ? rt.weekday : 0;
  char buf[40];
#if VM_LANG_ZH
  static const char* kWeekdayFullZh[] = {
    "星期日", "星期一", "星期二", "星期三",
    "星期四", "星期五", "星期六"
  };
  snprintf(buf, sizeof(buf), "%d月%d日 %s",
           rt.month, rt.day, kWeekdayFullZh[wd]);
#else
  static const char* kWeekdayFull[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
  };
  static const char* kMonthShort[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  const int mo = (rt.month  >= 1 && rt.month  <= 12) ? rt.month - 1 : 0;
  snprintf(buf, sizeof(buf), "%s, %s %d",
           kWeekdayFull[wd], kMonthShort[mo], rt.day);
#endif
  return String(buf);
}

String RtcClock::nowHeaderDateLabel()
{
  VoiceMemoRtcTime rt = {};
  if (!available_ || !readTime(rt) || !rt.voltageOK) return "--/--";

  const int wd = (rt.weekday >= 0 && rt.weekday < 7) ? rt.weekday : 0;
  char buf[24];
#if VM_LANG_ZH
  snprintf(buf, sizeof(buf), "%d月%d日 %s",
           rt.month, rt.day, kWeekdayNames[wd]);
#else
  static const char* kMonthShort[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  const int mo = (rt.month  >= 1 && rt.month  <= 12) ? rt.month - 1 : 0;
  snprintf(buf, sizeof(buf), "%s %02d %s",
           kMonthShort[mo], rt.day, kWeekdayNames[wd]);
#endif
  return String(buf);
}
