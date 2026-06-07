// AudioCapture.h -- PDM microphone capture into a single PSRAM WAV buffer.
//
// The reTerminal E-series uses a PDM microphone on GPIO41/42 with a power
// enable on GPIO38. This module owns three things and nothing else:
//   - One PSRAM-allocated WAV buffer big enough for the maximum recording.
//   - The ESP-IDF 5.x I2S PDM RX channel.
//   - The 44-byte WAV header that is written after recording stops.
//
// Lifecycle:
//   begin(sampleRate, maxSeconds): allocate buffer + open I2S channel.
//   startRecord(): reset the audio offset, ready to receive samples.
//   readChunk(): pull one DMA chunk; returns true if the buffer is now full.
//   finishRecord(): write the WAV header so wavData() can be uploaded.
//
// The class does NOT touch WiFi, LED, UI, or business logic. Adding a new
// codec (e.g. Opus) would only require changing this file.

#ifndef VOICE_MEMO_AUDIO_CAPTURE_H
#define VOICE_MEMO_AUDIO_CAPTURE_H

#include <Arduino.h>
#include <driver/i2s_common.h>

class AudioCapture {
 public:
  static constexpr size_t   kWavHeaderBytes = 44;
  static constexpr uint32_t kBitsPerSample  = 16;
  static constexpr uint32_t kChannels       = 1;

  AudioCapture();

  // Sets up the I2S PDM channel and allocates the buffer. clkPin/dataPin/
  // powerEnablePin are wired to GPIO42/41/38 on every E-series board, but
  // they stay parameters so the example does not hard-code anything that
  // could change on a different carrier board.
  bool begin(uint32_t sampleRate,
             uint32_t maxRecordSeconds,
             int clkPin,
             int dataPin,
             int powerEnablePin);

  // Recording state machine.
  void startRecord();
  // Pulls one chunk from the I2S DMA into the buffer. Returns true when the
  // buffer reached its max capacity and the caller should stop recording.
  bool readChunk();
  void finishRecord();   // Writes the WAV header for the data captured so far.

  // Buffer access. wavData() points to the buffer, wavSize() returns the
  // total number of bytes to upload (header + audio payload).
  const uint8_t* wavData() const { return wavBuffer_; }
  size_t   wavSize() const { return kWavHeaderBytes + audioBytes_; }
  size_t   audioBytes() const { return audioBytes_; }
  float    recordedSeconds() const;

  // True only when the audio payload is shorter than the minimum acceptable
  // recording (~1/3 second). Used by the app to ignore accidental taps.
  bool tooShort() const;

 private:
  static constexpr size_t kReadBufferBytes = 1024;
  static constexpr uint32_t kBytesPerSample = kBitsPerSample / 8;

  uint32_t bytesPerSecond() const;
  size_t   maxAudioBytes() const;
  void     writeWavHeader(uint32_t audioBytes);

  uint32_t sampleRate_;
  uint32_t maxRecordSeconds_;

  i2s_chan_handle_t rxHandle_;
  uint8_t* wavBuffer_;
  size_t   wavBufferBytes_;
  size_t   audioBytes_;
  uint8_t  readBuffer_[kReadBufferBytes];
};

#endif  // VOICE_MEMO_AUDIO_CAPTURE_H
