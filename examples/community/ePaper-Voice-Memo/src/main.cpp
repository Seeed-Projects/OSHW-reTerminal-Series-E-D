/*
 * VoiceMemoReminder -- hold KEY0 to record, release to create a memo.
 *
 * This file is the PlatformIO entry point and the ONLY file you have to edit
 * to make the device work on your network with your provider keys.
 * Everything else lives in dedicated modules:
 *
 *   VoiceMemoApp.*    application orchestrator (button, lifecycle)
 *   AudioCapture.*    PDM microphone + WAV buffer
 *   RtcClock.*        PCF8563 real-time clock
 *   MemoStore.*       reminder list + NVS persistence
 *   SpeechClient.*    speech-to-text upload
 *   MemoClient.*      LLM rewrite into {memo, due} JSON
 *   MemoUI.*          all e-paper drawing
 *   JsonUtil.h        tiny shared JSON helpers
 *   driver.h          device model + screen capability selector
 *
 * To add a new STT provider, edit SpeechClient.cpp and add one case.
 * To change the LLM prompt, edit MemoClient.cpp.
 * To redesign the screen, edit MemoUI.cpp.
 * Reminders persist across power cycles via NVS automatically.
 */

#include <Arduino.h>
#include <Preferences.h>
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#include "UiLang.h"
#include "VoiceMemoApp.h"

#ifndef VM_WIFI_SSID
#define VM_WIFI_SSID ""
#endif
#ifndef VM_WIFI_PASSWORD
#define VM_WIFI_PASSWORD ""
#endif
#ifndef VM_GROQ_API_KEY
#define VM_GROQ_API_KEY ""
#endif

static String nvsWifiSsid;
static String nvsWifiPass;
static String nvsApiKey;

static void loadNvsCredentials()
{
  Preferences prefs;
  if (!prefs.begin("config", true)) {
    nvsWifiSsid = VM_WIFI_SSID;
    nvsWifiPass = VM_WIFI_PASSWORD;
    nvsApiKey   = VM_GROQ_API_KEY;
    return;
  }
  nvsWifiSsid = prefs.getString("wifiSsid", VM_WIFI_SSID);
  nvsWifiPass = prefs.getString("wifiPass", VM_WIFI_PASSWORD);
  nvsApiKey   = prefs.getString("apiKey", VM_GROQ_API_KEY);
  prefs.end();
}

static VoiceMemoConfig buildConfig()
{
  return {
    .wifiSsid     = nvsWifiSsid.c_str(),
    .wifiPassword = nvsWifiPass.c_str(),

    .speech = {
      .provider = VM_SPEECH_OPENAI_COMPATIBLE,
      .url      = "https://api.groq.com/openai/v1/audio/transcriptions",
      .apiKey   = nvsApiKey.c_str(),
      .model    = "whisper-large-v3-turbo",
      .language = VM_LANG_ZH ? "zh" : "",
    },

    .memo = {
      .provider = VM_MEMO_OPENAI_COMPATIBLE,
      .url      = "https://api.groq.com/openai/v1/chat/completions",
      .apiKey   = nvsApiKey.c_str(),
      .model    = "llama-3.3-70b-versatile",
    },

    .audio = {
      .sampleRate       = 16000,
      .maxRecordSeconds = 20,
    },

    .httpTimeoutMs = 45000,
  };
}

static VoiceMemoApp* app;

void setup()
{
  loadNvsCredentials();
  static VoiceMemoConfig cfg = buildConfig();
  static VoiceMemoApp instance(cfg);
  app = &instance;
  app->begin();
}

void loop()
{
  if (app) app->loop();
}
