// SpeechClient.h -- speech-to-text uploader.
//
// The device records a WAV in PSRAM and hands it to this class. The class
// then talks to one of three backends:
//
//   - VM_SPEECH_OPENAI_COMPATIBLE: any provider that exposes the OpenAI
//     /audio/transcriptions endpoint (Groq, OpenAI, OpenRouter, self-hosted
//     Whisper).
//   - VM_SPEECH_DEEPGRAM: Deepgram's /v1/listen endpoint, which accepts a
//     raw WAV body and avoids multipart construction.
//   - VM_SPEECH_GATEWAY: the small Python service in gateway/ for offline
//     testing or for moving the API key off the device.
//
// To add a new provider, append a new enum value, write transcribeXxx() in
// the .cpp, and add one case to the dispatcher in transcribe().

#ifndef VOICE_MEMO_SPEECH_CLIENT_H
#define VOICE_MEMO_SPEECH_CLIENT_H

#include <Arduino.h>

enum VoiceMemoSpeechProvider {
  VM_SPEECH_GATEWAY,
  VM_SPEECH_OPENAI_COMPATIBLE,
  VM_SPEECH_DEEPGRAM,
};

struct SpeechConfig {
  VoiceMemoSpeechProvider provider;
  const char* url;
  const char* apiKey;
  const char* model;
  const char* language;
};

class SpeechClient {
 public:
  SpeechClient();

  void configure(const SpeechConfig& cfg, uint32_t httpTimeoutMs);

  // Uploads wavBuf (totalBytes) to the configured provider, places the
  // recognized text into outTranscript and returns true on success.
  // The caller must ensure WiFi is connected before calling.
  bool transcribe(const uint8_t* wavBuf, size_t totalBytes, String& outTranscript);

 private:
  bool transcribeOpenAICompatible(const uint8_t* wavBuf, size_t totalBytes, String& out);
  bool transcribeDeepgram(const uint8_t* wavBuf, size_t totalBytes, String& out);
  bool transcribeGateway(const uint8_t* wavBuf, size_t totalBytes, String& out);

  SpeechConfig cfg_;
  uint32_t     httpTimeoutMs_;
};

#endif  // VOICE_MEMO_SPEECH_CLIENT_H
