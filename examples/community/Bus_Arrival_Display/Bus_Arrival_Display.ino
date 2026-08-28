#include "driver.h"
#include "TFT_eSPI.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <sys/time.h>
#include <time.h>

// -----------------------------------------------------------------------------
// User configuration
// -----------------------------------------------------------------------------

enum class BusProvider {
  Singapore,
  HongKongKmb,  // KMB and Long Win Bus (LWB) stops.
};

// These values are supplied by the Firmware Hub before flashing and stored in
// the ESP32 NVS namespace "config". Do not add real credentials here.
struct UserConfig {
  String wifiSsid;
  String wifiPassword;
  BusProvider busProvider = BusProvider::Singapore;
  String busId;
  String busName;
  unsigned long refreshIntervalMs = 120000UL;
};

UserConfig userConfig;

// Singapore and China Standard Time are both UTC+8 with no daylight saving.
constexpr long UTC_OFFSET_SECONDS = 8 * 60 * 60;
constexpr int DAYLIGHT_OFFSET_SECONDS = 0;
constexpr char NTP_SERVER_PRIMARY[] = "pool.ntp.org";
constexpr char NTP_SERVER_FALLBACK[] = "time.cloudflare.com";

// Set the RTC from the build timestamp when its backup battery has lost power.
#define USE_COMPILE_TIME

// Used only when USE_COMPILE_TIME is disabled.
constexpr int INITIAL_YEAR = 2026;
constexpr int INITIAL_MONTH = 5;
constexpr int INITIAL_DAY = 26;
constexpr int INITIAL_HOUR = 14;
constexpr int INITIAL_MINUTE = 0;
constexpr int INITIAL_SECOND = 0;

// Uncomment temporarily to overwrite the RTC on every boot.
// #define FORCE_SET_TIME

// -----------------------------------------------------------------------------
// Hardware configuration
// -----------------------------------------------------------------------------

constexpr int PIN_BATTERY = A0;
constexpr int PIN_SERIAL_RX = 44;
constexpr int PIN_SERIAL_TX = 43;
constexpr int PIN_I2C_SDA = 19;
constexpr int PIN_I2C_SCL = 20;

constexpr uint8_t PCF8563_ADDRESS = 0x51;
constexpr uint8_t REG_CTRL1 = 0x00;
constexpr uint8_t REG_CTRL2 = 0x01;
constexpr uint8_t REG_SECONDS = 0x02;
constexpr uint8_t REG_CLKOUT = 0x0D;

constexpr int MIN_REFRESH_SECONDS = 30;
constexpr int MAX_REFRESH_SECONDS = 3600;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000UL;
constexpr unsigned long TIME_LOG_INTERVAL_MS = 2000UL;
constexpr size_t MAX_BUSES = 10;
constexpr int BUS_BADGE_WIDTH = 72;   // About four characters at text size 3.
constexpr int BUS_BADGE_HEIGHT = 32;

#define LOG Serial1

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

// -----------------------------------------------------------------------------
// Data models and global state
// -----------------------------------------------------------------------------

struct RtcTime {
  int year;
  int month;
  int day;
  int weekday;
  int hour;
  int minute;
  int second;
  bool voltageOK;
};

struct BusInfo {
  String route;
  String arrival;
  int eta;
};

BusInfo buses[MAX_BUSES];
size_t busCount = 0;

unsigned long lastTimeLogMs = 0;
unsigned long lastBusRefreshMs = 0;
bool applicationConfigured = false;

const char *const WEEKDAY_NAMES[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

// -----------------------------------------------------------------------------
// Firmware Hub configuration (ESP32 NVS)
// -----------------------------------------------------------------------------

static void loadUserConfig() {
  Preferences preferences;
  if (!preferences.begin("config", true)) {
    LOG.println("[CONFIG] ERROR: could not open NVS namespace 'config'.");
    return;
  }

  userConfig.wifiSsid = preferences.getString("wifiSsid", "");
  userConfig.wifiPassword = preferences.getString("wifiPassword", "");
  userConfig.busId = preferences.getString("busId", "");
  userConfig.busName = preferences.getString("busName", "");

  String provider = preferences.getString("busProvider", "singapore");
  provider.toLowerCase();
  if (provider == "hongkongkmb" || provider == "hong-kong-kmb" ||
      provider == "kmb") {
    userConfig.busProvider = BusProvider::HongKongKmb;
  } else {
    userConfig.busProvider = BusProvider::Singapore;
  }

  const int refreshSeconds = preferences.getInt("refreshSeconds", 120);
  const int boundedRefreshSeconds = constrain(refreshSeconds,
                                               MIN_REFRESH_SECONDS,
                                               MAX_REFRESH_SECONDS);
  userConfig.refreshIntervalMs =
      static_cast<unsigned long>(boundedRefreshSeconds) * 1000UL;
  preferences.end();

  LOG.printf("[CONFIG] Provider: %s, stop: %s, refresh: %d s\n",
             userConfig.busProvider == BusProvider::Singapore ? "Singapore" : "Hong Kong KMB/LWB",
             userConfig.busId.c_str(), boundedRefreshSeconds);
}

static bool hasRequiredUserConfig() {
  if (userConfig.wifiSsid.length() == 0 || userConfig.wifiPassword.length() == 0 ||
      userConfig.busId.length() == 0 || userConfig.busName.length() == 0) {
    LOG.println("[CONFIG] Missing required configuration. Reflash with Wi-Fi and bus fields filled in.");
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
// Generic helpers
// -----------------------------------------------------------------------------

static uint8_t bcdToDec(uint8_t bcd) {
  return static_cast<uint8_t>((bcd >> 4) * 10U + (bcd & 0x0FU));
}

static uint8_t decToBcd(uint8_t decimal) {
  return static_cast<uint8_t>((decimal / 10U << 4) | (decimal % 10U));
}

static float readBatteryVoltage() {
  delay(10);
  const int adcValue = analogRead(PIN_BATTERY);
  const float adcVoltage = adcValue * 3.3F / 4095.0F;
  return adcVoltage * 2.0F;
}

static int batteryPercent() {
  const float voltage = readBatteryVoltage();
  const int percent = static_cast<int>((voltage - 3.3F) / (4.2F - 3.3F) * 100.0F);
  return constrain(percent, 0, 100);
}

// -----------------------------------------------------------------------------
// PCF8563 RTC
// -----------------------------------------------------------------------------

static bool rtcReadRegisters(uint8_t reg, uint8_t *buffer, size_t length) {
  Wire.beginTransmission(PCF8563_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t received = Wire.requestFrom(PCF8563_ADDRESS, static_cast<uint8_t>(length));
  if (received != length) {
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    buffer[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

static bool rtcWriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(PCF8563_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool rtcProbe() {
  Wire.beginTransmission(PCF8563_ADDRESS);
  return Wire.endTransmission() == 0;
}

static bool rtcInitialize() {
  return rtcWriteRegister(REG_CTRL1, 0x00) &&
         rtcWriteRegister(REG_CTRL2, 0x00) &&
         rtcWriteRegister(REG_CLKOUT, 0x00);
}

static bool rtcVoltageOK() {
  uint8_t seconds = 0;
  return rtcReadRegisters(REG_SECONDS, &seconds, 1) && (seconds & 0x80U) == 0;
}

static bool rtcSetTime(int year, int month, int day, int hour, int minute, int second) {
  if (year < 2000 || year > 2099 || month < 1 || month > 12 ||
      day < 1 || day > 31 || hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 || second < 0 || second > 59) {
    return false;
  }

  tm date = {};
  date.tm_year = year - 1900;
  date.tm_mon = month - 1;
  date.tm_mday = day;
  mktime(&date);

  Wire.beginTransmission(PCF8563_ADDRESS);
  Wire.write(REG_SECONDS);
  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(day));
  Wire.write(static_cast<uint8_t>(date.tm_wday));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year % 100));
  return Wire.endTransmission() == 0;
}

static bool rtcGetTime(RtcTime &rtcTime) {
  uint8_t raw[7] = {};
  if (!rtcReadRegisters(REG_SECONDS, raw, sizeof(raw))) {
    return false;
  }

  rtcTime.voltageOK = (raw[0] & 0x80U) == 0;
  rtcTime.second = bcdToDec(raw[0] & 0x7FU);
  rtcTime.minute = bcdToDec(raw[1] & 0x7FU);
  rtcTime.hour = bcdToDec(raw[2] & 0x3FU);
  rtcTime.day = bcdToDec(raw[3] & 0x3FU);
  rtcTime.weekday = bcdToDec(raw[4] & 0x07U);
  rtcTime.month = bcdToDec(raw[5] & 0x1FU);

  const int year = bcdToDec(raw[6]);
  rtcTime.year = (raw[5] & 0x80U) ? 1900 + year : 2000 + year;
  return true;
}

static void syncSystemClock(const RtcTime &rtcTime) {
  tm date = {};
  date.tm_year = rtcTime.year - 1900;
  date.tm_mon = rtcTime.month - 1;
  date.tm_mday = rtcTime.day;
  date.tm_hour = rtcTime.hour;
  date.tm_min = rtcTime.minute;
  date.tm_sec = rtcTime.second;

  const timeval systemTime = {mktime(&date), 0};
  settimeofday(&systemTime, nullptr);
}

#ifdef USE_COMPILE_TIME
static void getCompileTime(int &year, int &month, int &day,
                           int &hour, int &minute, int &second) {
  constexpr char MONTH_NAMES[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  month = 1;
  for (int i = 0; i < 12; ++i) {
    if (strncmp(__DATE__, MONTH_NAMES + i * 3, 3) == 0) {
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
#endif

// -----------------------------------------------------------------------------
// Bus arrival API
// -----------------------------------------------------------------------------

static int minutesUntilIsoTime(const String &isoTime) {
  // KMB returns an ISO-8601 time such as 2026-08-05T12:34:00+08:00.
  int year, month, day, hour, minute, second;
  if (sscanf(isoTime.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day,
             &hour, &minute, &second) != 6) {
    return -1;
  }

  tm etaTime = {};
  etaTime.tm_year = year - 1900;
  etaTime.tm_mon = month - 1;
  etaTime.tm_mday = day;
  etaTime.tm_hour = hour;
  etaTime.tm_min = minute;
  etaTime.tm_sec = second;

  const time_t etaEpoch = mktime(&etaTime);
  if (etaEpoch == static_cast<time_t>(-1)) {
    return -1;
  }
  const long seconds = static_cast<long>(etaEpoch - time(nullptr));
  return seconds <= 0 ? 0 : static_cast<int>((seconds + 59) / 60);
}

static void parseSingaporeBusData(JsonDocument &document) {
  for (JsonObject bus : document["services"].as<JsonArray>()) {
    if (busCount >= MAX_BUSES) {
      break;
    }

    const String time = bus["next"]["time"].as<String>();
    if (time.length() < 16) {
      continue;
    }

    BusInfo &busInfo = buses[busCount];
    busInfo.route = bus["no"].as<String>();
    busInfo.arrival = time.substring(11, 16);
    busInfo.eta = bus["next"]["duration_ms"].as<long>() / 60000;

    LOG.printf("[BUS] %s  %s  %d min\n", busInfo.route.c_str(),
               busInfo.arrival.c_str(), busInfo.eta);
    ++busCount;
  }
}

static void parseHongKongKmbBusData(JsonDocument &document) {
  for (JsonObject bus : document["data"].as<JsonArray>()) {
    if (busCount >= MAX_BUSES) {
      break;
    }

    // The endpoint includes several arrivals per route. Show only the next one.
    if (bus["eta_seq"].as<int>() != 1) {
      continue;
    }

    const String etaTime = bus["eta"].as<String>();
    const int etaMinutes = minutesUntilIsoTime(etaTime);
    if (etaMinutes < 0) {
      continue;  // No real-time estimate is currently available.
    }

    BusInfo &busInfo = buses[busCount];
    busInfo.route = bus["route"].as<String>();
    busInfo.arrival = etaTime.substring(11, 16);
    busInfo.eta = etaMinutes;

    LOG.printf("[BUS] %s  %s  %d min\n", busInfo.route.c_str(),
               busInfo.arrival.c_str(), busInfo.eta);
    ++busCount;
  }
}

static void parseBusData(const String &json) {
  // A busy KMB/LWB interchange returns more ETA records than a Singapore stop.
  DynamicJsonDocument document(24576);
  const DeserializationError error = deserializeJson(document, json);
  if (error) {
    LOG.println("[BUS] JSON parse error");
    return;
  }

  busCount = 0;
  if (userConfig.busProvider == BusProvider::Singapore) {
    parseSingaporeBusData(document);
  } else {
    parseHongKongKmbBusData(document);
  }
}

static void updateBusData() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url;
  if (userConfig.busProvider == BusProvider::Singapore) {
    url = String("https://arrivelah2.busrouter.sg/?id=") + userConfig.busId;
  } else {
    url = String("https://data.etabus.gov.hk/v1/transport/kmb/stop-eta/") +
          userConfig.busId;
  }
  LOG.println(url);

  if (!http.begin(client, url)) {
    LOG.println("[BUS] HTTP client initialization failed");
    return;
  }

  const int httpCode = http.GET();
  LOG.printf("[BUS] HTTP code: %d\n", httpCode);
  if (httpCode == HTTP_CODE_OK) {
    parseBusData(http.getString());
  }
  http.end();
}

// -----------------------------------------------------------------------------
// Display
// -----------------------------------------------------------------------------

static void drawBusIcon(int x, int y) {
#ifdef EPAPER_ENABLE
  constexpr int ICON_WIDTH = 96;
  constexpr int ICON_HEIGHT = 52;
  constexpr int WHEEL_RADIUS = 7;

  // A simple high-contrast bus silhouette for the monochrome e-paper display.
  epaper.fillRect(x, y, ICON_WIDTH, ICON_HEIGHT - 10, TFT_BLACK);
  epaper.fillRect(x + 8, y + 8, 46, 15, TFT_WHITE);   // windows
  epaper.fillRect(x + 61, y + 8, 24, 29, TFT_WHITE);  // door
  epaper.fillRect(x + 65, y + 11, 16, 23, TFT_BLACK); // door interior

  epaper.fillCircle(x + 22, y + ICON_HEIGHT - 7, WHEEL_RADIUS, TFT_BLACK);
  epaper.fillCircle(x + 74, y + ICON_HEIGHT - 7, WHEEL_RADIUS, TFT_BLACK);
  epaper.fillCircle(x + 22, y + ICON_HEIGHT - 7, 3, TFT_WHITE);
  epaper.fillCircle(x + 74, y + ICON_HEIGHT - 7, 3, TFT_WHITE);
#endif
}

static void drawStopName() {
#ifdef EPAPER_ENABLE
  constexpr int STOP_NAME_X = 40;
  constexpr int STOP_NAME_Y = 270;
  constexpr int STOP_NAME_MAX_WIDTH = 300;
  constexpr int STOP_NAME_LINE_HEIGHT = 42;
  constexpr int STOP_NAME_MAX_LINES = 2;

  String remaining(userConfig.busName);
  for (int lineNumber = 0; lineNumber < STOP_NAME_MAX_LINES &&
                           remaining.length() > 0;
       ++lineNumber) {
    String line;
    int lastSpace = -1;
    size_t index = 0;
    for (; index < remaining.length(); ++index) {
      const char character = remaining[index];
      const String candidate = line + character;
      if (epaper.textWidth(candidate) > STOP_NAME_MAX_WIDTH) {
        break;
      }
      line = candidate;
      if (character == ' ') {
        lastSpace = static_cast<int>(index);
      }
    }

    if (index < remaining.length()) {
      // Prefer a word boundary, but still split a very long single word.
      if (lastSpace > 0) {
        line = remaining.substring(0, lastSpace);
        remaining = remaining.substring(lastSpace + 1);
      } else {
        remaining = remaining.substring(index);
      }
    } else {
      remaining = "";
    }

    if (lineNumber == STOP_NAME_MAX_LINES - 1 && remaining.length() > 0) {
      while (line.length() > 0 &&
             epaper.textWidth(line + "...") > STOP_NAME_MAX_WIDTH) {
        line.remove(line.length() - 1);
      }
      line += "...";
    }

    epaper.drawString(line, STOP_NAME_X,
                      STOP_NAME_Y + lineNumber * STOP_NAME_LINE_HEIGHT);
  }
#endif
}

static void displayBusTimeOnEPaper() {
#ifdef EPAPER_ENABLE
  RtcTime rtcTime;
  if (!rtcGetTime(rtcTime)) {
    return;
  }

  epaper.fillScreen(TFT_WHITE);

  char timeText[8];
  snprintf(timeText, sizeof(timeText), "%02d:%02d", rtcTime.hour, rtcTime.minute);
  epaper.setTextSize(8);
  epaper.drawString(timeText, 40, 60);

  epaper.setTextSize(4);
  epaper.drawString(WEEKDAY_NAMES[rtcTime.weekday], 40, 160);

  char dateText[16];
  snprintf(dateText, sizeof(dateText), "%04d-%02d-%02d", rtcTime.year,
           rtcTime.month, rtcTime.day);
  epaper.drawString(dateText, 40, 220);
  drawStopName();
  drawBusIcon(40, 380);

  epaper.drawString("Bus\f\f\fETA", 370, 20);
  int y = 70;
  // Render every parsed entry, including the final ETA row. busCount is capped
  // at MAX_BUSES by the parsers, so this also remains within the array bounds.
  for (size_t i = 0; i < busCount; ++i) {
    // Show the route number in a fixed-width, high-contrast badge.
    epaper.fillRect(370, y, BUS_BADGE_WIDTH, BUS_BADGE_HEIGHT, TFT_BLACK);
    epaper.setTextSize(3);
    epaper.setTextColor(TFT_WHITE, TFT_BLACK);
    epaper.drawString(buses[i].route, 376, y + 3);

    char arrivalText[40];
    snprintf(arrivalText, sizeof(arrivalText), "%s   %dmin",
             buses[i].arrival.c_str(), buses[i].eta);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString(arrivalText, 500, y + 3);
    y += 45;
  }

  epaper.update();
  LOG.println("[EPAPER] Refresh complete");
#endif
}

// -----------------------------------------------------------------------------
// Application lifecycle
// -----------------------------------------------------------------------------

static void initializeRtc() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000UL);

  if (!rtcProbe()) {
    LOG.println("[RTC] FATAL: PCF8563 not found. Check wiring and backup battery.");
    while (true) {
      delay(1000);
    }
  }

  if (!rtcInitialize()) {
    LOG.println("[RTC] FATAL: PCF8563 initialization failed.");
    while (true) {
      delay(1000);
    }
  }

#ifdef FORCE_SET_TIME
  const bool setTime = true;
  LOG.println("[RTC] FORCE_SET_TIME enabled; overwriting RTC time.");
#else
  const bool setTime = !rtcVoltageOK();
#endif

  if (setTime) {
#ifdef USE_COMPILE_TIME
    int year, month, day, hour, minute, second;
    getCompileTime(year, month, day, hour, minute, second);
    rtcSetTime(year, month, day, hour, minute, second);
#else
    rtcSetTime(INITIAL_YEAR, INITIAL_MONTH, INITIAL_DAY, INITIAL_HOUR,
               INITIAL_MINUTE, INITIAL_SECOND);
#endif
  }

  RtcTime rtcTime;
  if (rtcGetTime(rtcTime)) {
    syncSystemClock(rtcTime);
    LOG.printf("[RTC] %04d-%02d-%02d (%s) %02d:%02d:%02d\n", rtcTime.year,
               rtcTime.month, rtcTime.day, WEEKDAY_NAMES[rtcTime.weekday],
               rtcTime.hour, rtcTime.minute, rtcTime.second);
  } else {
    LOG.println("[RTC] ERROR: could not read time.");
  }
}

static void showStartupError(const char *title, const char *detail) {
#ifdef EPAPER_ENABLE
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextSize(3);
  epaper.drawString(title, 40, 250);
  epaper.setTextSize(2);
  epaper.drawString(detail, 40, 310);
  epaper.update();
#endif
}

static bool connectWiFi() {
  WiFi.begin(userConfig.wifiSsid.c_str(), userConfig.wifiPassword.c_str());
  LOG.print("[WIFI] Connecting");
  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    LOG.print('.');
  }

  if (WiFi.status() != WL_CONNECTED) {
    LOG.println("\n[WIFI] ERROR: connection timed out. Check the Hub Wi-Fi settings.");
    WiFi.disconnect();
    return false;
  }

  LOG.printf("\n[WIFI] Connected: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

static bool syncTimeFromNtp() {
  LOG.println("[NTP] Synchronizing time...");

  // initializeRtc() sets the system clock from the RTC before Wi-Fi connects.
  // Clear that provisional value first: otherwise getLocalTime() may return the
  // old RTC value immediately instead of waiting for an actual NTP response.
  RtcTime previousRtcTime;
  const bool hasPreviousRtcTime = rtcGetTime(previousRtcTime);
  const timeval unsetTime = {0, 0};
  settimeofday(&unsetTime, nullptr);

  configTime(UTC_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS, NTP_SERVER_PRIMARY,
             NTP_SERVER_FALLBACK);

  tm timeInfo = {};
  if (!getLocalTime(&timeInfo, 15000)) {
    LOG.println("[NTP] ERROR: time synchronization timed out; keeping RTC time.");
    if (hasPreviousRtcTime) {
      syncSystemClock(previousRtcTime);
    }
    return false;
  }

  const int year = timeInfo.tm_year + 1900;
  const int month = timeInfo.tm_mon + 1;
  const bool rtcUpdated = rtcSetTime(year, month, timeInfo.tm_mday,
                                     timeInfo.tm_hour, timeInfo.tm_min,
                                     timeInfo.tm_sec);
  if (!rtcUpdated) {
    LOG.println("[NTP] ERROR: received time, but could not update RTC.");
    return false;
  }

  LOG.printf("[NTP] Time synchronized: %04d-%02d-%02d %02d:%02d:%02d\n",
             year, month, timeInfo.tm_mday, timeInfo.tm_hour,
             timeInfo.tm_min, timeInfo.tm_sec);
  return true;
}

static void logCurrentTime() {
  RtcTime rtcTime;
  if (!rtcGetTime(rtcTime)) {
    LOG.println("[RTC] ERROR: could not read time.");
    return;
  }

  LOG.printf("[TIME] %04d-%02d-%02d (%s) %02d:%02d:%02d%s\n", rtcTime.year,
             rtcTime.month, rtcTime.day, WEEKDAY_NAMES[rtcTime.weekday],
             rtcTime.hour, rtcTime.minute, rtcTime.second,
             rtcTime.voltageOK ? "" : " [VL: battery low]");
}

void setup() {
#ifdef EPAPER_ENABLE
  epaper.begin();
  epaper.fillScreen(TFT_WHITE);
  epaper.update();
#endif

  LOG.begin(115200, SERIAL_8N1, PIN_SERIAL_RX, PIN_SERIAL_TX);
  delay(500);
  LOG.println("\n=== reTerminal E1001 Bus Display ===");

  loadUserConfig();
  if (!hasRequiredUserConfig()) {
    showStartupError("Setup required", "Configure in Firmware Hub");
    return;
  }

  initializeRtc();
  if (!connectWiFi()) {
    showStartupError("Wi-Fi unavailable", "Check settings and retry");
    return;
  }
  applicationConfigured = true;
  syncTimeFromNtp();
  updateBusData();
  displayBusTimeOnEPaper();
}

void loop() {
  if (!applicationConfigured) {
    delay(1000);
    return;
  }

  const unsigned long now = millis();

  if (now - lastBusRefreshMs >= userConfig.refreshIntervalMs) {
    lastBusRefreshMs = now;
    LOG.println("[BUS] Updating...");
    updateBusData();
    displayBusTimeOnEPaper();
  }

  if (now - lastTimeLogMs >= TIME_LOG_INTERVAL_MS) {
    lastTimeLogMs = now;
    logCurrentTime();//A function that can print the current time at the serial port every two seconds.
  }
}
