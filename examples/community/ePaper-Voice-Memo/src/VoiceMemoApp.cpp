#include "VoiceMemoApp.h"

#include <WiFi.h>

#include "BatteryMath.h"
#include "UiLang.h"

VoiceMemoApp::VoiceMemoApp(const VoiceMemoConfig& config)
  : config_(config),
    audio_(),
    rtc_(),
    store_(),
    stt_(),
    memo_(),
    quote_(),
    ui_(),
    touch_(),
    recording_(false),
    busy_(false),
    lastRawButton_(HIGH),
    stableButton_(HIGH),
    ledState_(false),
    debounceMs_(0),
    lastBlinkMs_(0),
    lastListRefreshMs_(0)
{
}

void VoiceMemoApp::ledOn()  { digitalWrite(VM_LED_PIN, LOW); }
void VoiceMemoApp::ledOff() { digitalWrite(VM_LED_PIN, HIGH); }

void VoiceMemoApp::beepStart()
{
  // Short, brighter beep at recording start.
  // 短促且更明显的录音开始提示音。
  tone(kBuzzerPin, 2500, 180);
}

void VoiceMemoApp::setupPins()
{
  pinMode(VM_LED_PIN, OUTPUT);
  ledOff();
  pinMode(kKey0Pin, INPUT);
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);

  pinMode(kBatteryEnablePin, OUTPUT);
  digitalWrite(kBatteryEnablePin, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(kBatteryAdcPin, ADC_11db);
}

int VoiceMemoApp::readBatteryPercent()
{
  digitalWrite(kBatteryEnablePin, HIGH);
  delay(5);
  const int mv = analogReadMilliVolts(kBatteryAdcPin);
  digitalWrite(kBatteryEnablePin, LOW);
  return vmBatteryPercent(mv);
}

UiStatus VoiceMemoApp::currentStatus(bool processing)
{
  UiStatus s;
  s.wifiConnected  = (WiFi.status() == WL_CONNECTED);
  s.batteryPercent = readBatteryPercent();
  s.processing     = processing;
  return s;
}

void VoiceMemoApp::drawTodoList(const String& hint, bool processing,
                                bool allowQuoteNetwork)
{
  const time_t nowEpoch = rtc_.nowEpoch();
  bool quoteNetworkReady = false;
  if (allowQuoteNetwork && quote_.needsRefresh(nowEpoch)) {
    quoteNetworkReady = (WiFi.status() == WL_CONNECTED) || ensureWiFi(5000);
  }
  quote_.refreshIfNeeded(nowEpoch, quoteNetworkReady);
  ui_.drawTodoList(store_, rtc_, currentStatus(processing), hint, quote_.quote());
  lastListRefreshMs_ = millis();
}

bool VoiceMemoApp::ensureWiFi(uint32_t timeoutMs)
{
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(config_.wifiSsid, config_.wifiPassword);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial1.print(".");
  }
  Serial1.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial1.printf("[wifi] connected: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial1.println("[wifi] connection failed");
  return false;
}

void VoiceMemoApp::begin()
{
  // Arduino startup order:
  //   1. Start the debug UART (Serial1 on GPIO43/44).
  //   2. Configure pins (LED + KEY0 + buzzer).
  //   3. Initialize the I2C real-time clock (also brings up the shared
  //      I2C bus that the touch controller uses).
  //   4. Configure the e-paper display and show the BOOT splash.
  //   5. Allocate the WAV buffer and start the PDM microphone.
  //   6. Probe the touch controller now that the bus exists and the panel
  //      size is known.
  //   7. Load persisted reminders; configure speech / memo clients.
  //   8. Try WiFi once, then draw the reminder list.
  Serial1.begin(115200, SERIAL_8N1, kSerialRxPin, kSerialTxPin);
  delay(500);

  setupPins();
  const bool rtcOk = rtc_.begin(kI2cSdaPin, kI2cSclPin);

  Serial1.println("=========================================");
  Serial1.println("  VoiceMemoReminder");
  Serial1.printf("  Device: %s\n", VM_DEVICE_NAME);
  Serial1.printf("  RTC:    %s\n", rtcOk ? "ok" : "unavailable");
  Serial1.println("=========================================");

  ui_.begin();
  ui_.drawBoot(rtc_, uiStr(UiStringId::kBootStarting), currentStatus(false));

  if (!audio_.begin(config_.audio.sampleRate, config_.audio.maxRecordSeconds,
                    kMicClkPin, kMicDataPin, kMicPwrEnPin)) {
    // TODO(i18n): hardware-fault diagnostics stay in English (contain code
    // identifiers like PSRAM / driver.h and surface only on a boot failure).
    ui_.drawStatus("ERR", "Mic failed",
                   "Audio buffer or PDM microphone init failed. Check OPI PSRAM and driver.h.",
                   "Board: XIAO ESP32S3, PSRAM: OPI PSRAM.", false, 0.0f);
    while (true) delay(1000);
  }

#if VM_HAS_TOUCH
  touch_.begin(kTouchIntPin, kTouchResetPin,
               ui_.displayWidth(), ui_.displayHeight());
#endif

  store_.begin();
  stt_.configure(config_.speech, config_.httpTimeoutMs);
  memo_.configure(config_.memo,  config_.httpTimeoutMs);
  quote_.configure(config_.memo, config_.httpTimeoutMs);
  quote_.begin(rtc_.nowEpoch());

  ui_.drawBoot(rtc_, uiStr(UiStringId::kBootWifi), currentStatus(false));
  ensureWiFi(15000);

  drawTodoList(uiStr(UiStringId::kHintAdd), false, true);
}

void VoiceMemoApp::startRecording()
{
  if (busy_ || recording_) return;

  // CRITICAL: capture must begin IMMEDIATELY. An ePaper full refresh costs
  // ~1.5 s on E1003, but the I2S DMA ring can only buffer ~256 ms of audio.
  // If we drew a "REC" screen here, the first second of the user's speech
  // would be overwritten in DMA before captureChunk() ever ran. So we do
  // NOT touch the screen at the start of a recording -- the buzzer beep
  // and solid LED are the user feedback.
  audio_.startRecord();
  recording_ = true;
  beepStart();
  ledOn();

  Serial1.println("[rec] start");
}

void VoiceMemoApp::stopRecording(bool forced)
{
  if (!recording_) return;
  recording_ = false;
  ledOff();
  busy_ = true;

  const float seconds = audio_.recordedSeconds();
  Serial1.printf("[rec] stop: %.2fs, %u audio bytes\n", seconds,
                 static_cast<unsigned>(audio_.audioBytes()));

  if (audio_.tooShort()) {
    drawTodoList(uiStr(UiStringId::kHintTooShort), false, false);
    busy_ = false;
    return;
  }

  audio_.finishRecord();

  // Screen refresh is safe here because audio capture is already complete.
  // Unlike at recording START, where an ePaper refresh would starve the I2S
  // DMA ring and lose audio samples.
  // Inline processing state: keep the list visible, show "Processing" in the
  // header. Safe to refresh here -- audio capture is already complete.
  drawTodoList(uiStr(UiStringId::kHintAdd), true, false);
  ledOn();   // solid LED through the network call as a second cue

  if (!ensureWiFi(10000)) {
    ledOff();
    drawTodoList(uiStr(UiStringId::kHintNoWifi), false, false);
    busy_ = false;
    return;
  }

  String transcript;
  const bool sttOk = stt_.transcribe(audio_.wavData(), audio_.wavSize(),
                                     transcript);
  if (!sttOk && transcript.length() == 0) {
    transcript = uiStr(UiStringId::kNoSpeech);
  }
  Serial1.printf("[stt] \"%s\"\n", transcript.c_str());

  const time_t nowEpoch = rtc_.nowEpoch();
  MemoEntry entry = memo_.summarize(transcript, nowEpoch);
  Serial1.printf("[memo] \"%s\" due=%lld label=\"%s\"\n",
                 entry.text.c_str(), static_cast<long long>(entry.dueEpoch),
                 entry.fuzzyLabel.c_str());

#if VM_HAS_TOUCH
  store_.add(entry);
#else
  store_.sortByDue(nowEpoch);
  store_.addWithinVisibleLimit(entry, VM_VISIBLE_MEMO_MAX);
#endif
  ledOff();

  const String hint = forced
      ? uiStr(UiStringId::kHintMaxLen)
      : uiStr(UiStringId::kHintAdd);
  drawTodoList(hint, false, true);

  busy_ = false;
}

void VoiceMemoApp::captureChunk()
{
  if (!recording_) return;

  const bool full = audio_.readChunk();
  if (full) {
    stopRecording(true);
    return;
  }

  const unsigned long now = millis();
  if (now - lastBlinkMs_ >= 300) {
    lastBlinkMs_ = now;
    ledState_ = !ledState_;
    if (ledState_) ledOn(); else ledOff();
  }
}

void VoiceMemoApp::pollButton()
{
  // KEY0 is active low (hardware pull-up). Debounce converts the raw GPIO
  // into clean press / release events: press starts recording, release
  // stops the recording and triggers upload + summarize + render.
  const bool rawButton = digitalRead(kKey0Pin);
  if (rawButton != lastRawButton_) {
    debounceMs_ = millis();
    lastRawButton_ = rawButton;
  }
  if ((millis() - debounceMs_) > kDebounceDelayMs && rawButton != stableButton_) {
    stableButton_ = rawButton;
    if (stableButton_ == LOW) startRecording();
    else                      stopRecording(false);
  }
}

void VoiceMemoApp::pollTouch()
{
#if !VM_HAS_TOUCH
  return;
#else
  // Ignore touches during recording / network calls so a stray finger does
  // not interrupt the current operation.
  if (recording_ || busy_) return;
  if (!touch_.available()) return;

  uint16_t tx = 0, ty = 0;
  if (!touch_.poll(&tx, &ty)) return;

  const int idx = ui_.hitTestCheckbox(tx, ty);
  if (idx < 0) return;

  Serial1.printf("[touch] toggle row %d\n", idx);
  store_.toggleDone(static_cast<size_t>(idx));
  drawTodoList(uiStr(UiStringId::kHintAdd), false, false);
#endif
}

void VoiceMemoApp::pollScheduledRefresh()
{
  if (recording_ || busy_) return;
  const unsigned long now = millis();
  if (now - lastListRefreshMs_ < kListRefreshMs) return;

  // KEY0 wins over the timed e-paper refresh, including the debounce window.
  // KEY0 优先级高于定时刷屏  包括按键防抖尚未稳定的短窗口。
  const bool rawButton = digitalRead(kKey0Pin);
  const bool key0MayBeActive =
      rawButton == LOW
      || lastRawButton_ == LOW
      || stableButton_ == LOW
      || rawButton != stableButton_
      || now - debounceMs_ <= kDebounceDelayMs;
  if (key0MayBeActive) return;

  drawTodoList(uiStr(UiStringId::kHintAdd), false, true);
}

void VoiceMemoApp::loop()
{
  pollButton();
  captureChunk();
  pollTouch();
  pollScheduledRefresh();
}
