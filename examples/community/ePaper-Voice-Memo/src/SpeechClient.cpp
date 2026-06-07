#include "SpeechClient.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_random.h>

#include "JsonUtil.h"

namespace {

bool beginHttp(HTTPClient& http, WiFiClientSecure& secure, const char* url)
{
  const String target(url);
  if (target.startsWith("https://")) {
    // This is an example, so certificate provisioning is intentionally left
    // out to keep first-run setup simple. Production firmware should pin a
    // CA cert or public key instead of using setInsecure().
    secure.setInsecure();
    return http.begin(secure, target);
  }
  return http.begin(target);
}

void appendString(uint8_t* dst, size_t& off, const String& text)
{
  memcpy(dst + off, text.c_str(), text.length());
  off += text.length();
}

void appendBytes(uint8_t* dst, size_t& off, const uint8_t* data, size_t len)
{
  memcpy(dst + off, data, len);
  off += len;
}

}  // namespace

SpeechClient::SpeechClient()
  : cfg_{},
    httpTimeoutMs_(45000)
{
}

void SpeechClient::configure(const SpeechConfig& cfg, uint32_t httpTimeoutMs)
{
  cfg_ = cfg;
  httpTimeoutMs_ = httpTimeoutMs;
}

bool SpeechClient::transcribe(const uint8_t* wavBuf, size_t totalBytes,
                              String& outTranscript)
{
  switch (cfg_.provider) {
    case VM_SPEECH_OPENAI_COMPATIBLE:
      return transcribeOpenAICompatible(wavBuf, totalBytes, outTranscript);
    case VM_SPEECH_DEEPGRAM:
      return transcribeDeepgram(wavBuf, totalBytes, outTranscript);
    case VM_SPEECH_GATEWAY:
    default:
      return transcribeGateway(wavBuf, totalBytes, outTranscript);
  }
}

bool SpeechClient::transcribeOpenAICompatible(const uint8_t* wavBuf,
                                              size_t totalBytes,
                                              String& out)
{
  // Build a multipart/form-data body in PSRAM:
  //   field "model"     -> configured Whisper variant
  //   field "language"  -> optional ISO hint
  //   field "response_format" -> json
  //   field "file"      -> the WAV body itself
  const String boundary = "----voice-memo-" +
                          String(static_cast<uint32_t>(esp_random()), HEX);

  String prefix;
  prefix.reserve(512);
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
  prefix += cfg_.model;
  prefix += "\r\n";
  if (cfg_.language && strlen(cfg_.language) > 0) {
    prefix += "--" + boundary + "\r\n";
    prefix += "Content-Disposition: form-data; name=\"language\"\r\n\r\n";
    prefix += cfg_.language;
    prefix += "\r\n";
  }
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; name=\"response_format\"\r\n\r\n";
  prefix += "json\r\n";
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; name=\"file\"; filename=\"memo.wav\"\r\n";
  prefix += "Content-Type: audio/wav\r\n\r\n";

  const String suffix = "\r\n--" + boundary + "--\r\n";
  const size_t bodyBytes = prefix.length() + totalBytes + suffix.length();
  uint8_t* body = static_cast<uint8_t*>(ps_malloc(bodyBytes));
  if (!body) body = static_cast<uint8_t*>(malloc(bodyBytes));
  if (!body) {
    out = "Cannot allocate cloud upload body.";
    return false;
  }

  size_t offset = 0;
  appendString(body, offset, prefix);
  appendBytes(body, offset, wavBuf, totalBytes);
  appendString(body, offset, suffix);

  HTTPClient http;
  WiFiClientSecure secure;
  http.setTimeout(httpTimeoutMs_);
  if (!beginHttp(http, secure, cfg_.url)) {
    free(body);
    out = "Speech URL is invalid.";
    return false;
  }

  http.addHeader("Authorization", String("Bearer ") + cfg_.apiKey);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  const int code = http.POST(body, bodyBytes);
  free(body);

  const String response = http.getString();
  http.end();
  Serial1.printf("[stt/openai] status=%d bytes=%u\n",
                 code, static_cast<unsigned>(response.length()));

  if (code != 200) {
    out = "Speech service HTTP " + String(code);
    return false;
  }
  out = voice_memo::jsonStringValue(response, "text");
  return out.length() > 0;
}

bool SpeechClient::transcribeDeepgram(const uint8_t* wavBuf, size_t totalBytes,
                                      String& out)
{
  HTTPClient http;
  WiFiClientSecure secure;
  http.setTimeout(httpTimeoutMs_);
  if (!beginHttp(http, secure, cfg_.url)) {
    out = "Deepgram URL is invalid.";
    return false;
  }

  http.addHeader("Authorization", String("Token ") + cfg_.apiKey);
  http.addHeader("Content-Type", "audio/wav");
  const int code = http.POST(const_cast<uint8_t*>(wavBuf), totalBytes);
  const String response = http.getString();
  http.end();
  Serial1.printf("[stt/deepgram] status=%d bytes=%u\n",
                 code, static_cast<unsigned>(response.length()));

  if (code < 200 || code >= 300) {
    out = "Speech service HTTP " + String(code);
    return false;
  }
  out = voice_memo::jsonStringValue(response, "transcript");
  return out.length() > 0;
}

bool SpeechClient::transcribeGateway(const uint8_t* wavBuf, size_t totalBytes,
                                     String& out)
{
  HTTPClient http;
  WiFiClientSecure secure;
  http.setTimeout(httpTimeoutMs_);
  if (!beginHttp(http, secure, cfg_.url)) {
    out = "Gateway URL is invalid.";
    return false;
  }

  http.addHeader("Content-Type", "audio/wav");
  const int code = http.POST(const_cast<uint8_t*>(wavBuf), totalBytes);
  const String response = http.getString();
  http.end();
  Serial1.printf("[stt/gateway] status=%d bytes=%u\n",
                 code, static_cast<unsigned>(response.length()));

  if (code != 200) {
    out = "Gateway error HTTP " + String(code);
    return false;
  }
  // The gateway returns {"transcript":"..."} and may also pre-summarize as
  // {"memo":"..."}. The memo client will handle summarization either way, so
  // we only need the raw transcript here.
  out = voice_memo::jsonStringValue(response, "transcript");
  if (out.length() == 0) out = voice_memo::jsonStringValue(response, "memo");
  return out.length() > 0;
}
