#include "AudioCapture.h"

#include <driver/i2s_pdm.h>

namespace {

void putLE16(uint8_t* dst, uint16_t value)
{
  dst[0] = static_cast<uint8_t>(value);
  dst[1] = static_cast<uint8_t>(value >> 8);
}

void putLE32(uint8_t* dst, uint32_t value)
{
  dst[0] = static_cast<uint8_t>(value);
  dst[1] = static_cast<uint8_t>(value >> 8);
  dst[2] = static_cast<uint8_t>(value >> 16);
  dst[3] = static_cast<uint8_t>(value >> 24);
}

}  // namespace

AudioCapture::AudioCapture()
  : sampleRate_(16000),
    maxRecordSeconds_(20),
    rxHandle_(nullptr),
    wavBuffer_(nullptr),
    wavBufferBytes_(0),
    audioBytes_(0),
    readBuffer_{}
{
}

uint32_t AudioCapture::bytesPerSecond() const
{
  return sampleRate_ * kChannels * kBytesPerSample;
}

size_t AudioCapture::maxAudioBytes() const
{
  return static_cast<size_t>(bytesPerSecond()) * maxRecordSeconds_;
}

float AudioCapture::recordedSeconds() const
{
  return static_cast<float>(audioBytes_) / static_cast<float>(bytesPerSecond());
}

bool AudioCapture::tooShort() const
{
  return audioBytes_ < bytesPerSecond() / 3;
}

void AudioCapture::writeWavHeader(uint32_t audioBytes)
{
  memcpy(wavBuffer_ + 0, "RIFF", 4);
  putLE32(wavBuffer_ + 4, 36 + audioBytes);
  memcpy(wavBuffer_ + 8, "WAVE", 4);
  memcpy(wavBuffer_ + 12, "fmt ", 4);
  putLE32(wavBuffer_ + 16, 16);
  putLE16(wavBuffer_ + 20, 1);
  putLE16(wavBuffer_ + 22, kChannels);
  putLE32(wavBuffer_ + 24, sampleRate_);
  putLE32(wavBuffer_ + 28, bytesPerSecond());
  putLE16(wavBuffer_ + 32, kChannels * kBytesPerSample);
  putLE16(wavBuffer_ + 34, kBitsPerSample);
  memcpy(wavBuffer_ + 36, "data", 4);
  putLE32(wavBuffer_ + 40, audioBytes);
}

bool AudioCapture::begin(uint32_t sampleRate,
                         uint32_t maxRecordSeconds,
                         int clkPin,
                         int dataPin,
                         int powerEnablePin)
{
  sampleRate_ = sampleRate;
  maxRecordSeconds_ = maxRecordSeconds;

  // PSRAM is preferred because 20 seconds at 16 kHz mono is ~640 KB. malloc
  // is kept as a fallback for very short test configurations.
  wavBufferBytes_ = kWavHeaderBytes + maxAudioBytes();
  wavBuffer_ = static_cast<uint8_t*>(ps_malloc(wavBufferBytes_));
  if (!wavBuffer_) wavBuffer_ = static_cast<uint8_t*>(malloc(wavBufferBytes_));
  if (!wavBuffer_) return false;

  pinMode(powerEnablePin, OUTPUT);
  digitalWrite(powerEnablePin, HIGH);
  delay(50);

  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chanCfg.dma_desc_num = 8;
  chanCfg.dma_frame_num = 512;
  chanCfg.auto_clear = true;

  if (i2s_new_channel(&chanCfg, nullptr, &rxHandle_) != ESP_OK) return false;

  i2s_pdm_rx_config_t pdmCfg = {};
  pdmCfg.clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sampleRate_);
  pdmCfg.slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                   I2S_SLOT_MODE_MONO);
  pdmCfg.gpio_cfg.clk = static_cast<gpio_num_t>(clkPin);
  pdmCfg.gpio_cfg.din = static_cast<gpio_num_t>(dataPin);
  pdmCfg.gpio_cfg.invert_flags.clk_inv = false;

  if (i2s_channel_init_pdm_rx_mode(rxHandle_, &pdmCfg) != ESP_OK) {
    i2s_del_channel(rxHandle_);
    rxHandle_ = nullptr;
    return false;
  }
  if (i2s_channel_enable(rxHandle_) != ESP_OK) {
    i2s_del_channel(rxHandle_);
    rxHandle_ = nullptr;
    return false;
  }

  // Drop the first few DMA buffers so the PDM decimation filter warms up.
  size_t dummy = 0;
  for (int i = 0; i < 3; i++) {
    i2s_channel_read(rxHandle_, readBuffer_, sizeof(readBuffer_), &dummy,
                     pdMS_TO_TICKS(300));
  }
  return true;
}

void AudioCapture::startRecord()
{
  audioBytes_ = 0;
  if (wavBuffer_) memset(wavBuffer_, 0, kWavHeaderBytes);
}

bool AudioCapture::readChunk()
{
  if (!rxHandle_ || !wavBuffer_) return true;

  size_t bytesRead = 0;
  const esp_err_t err = i2s_channel_read(rxHandle_, readBuffer_, sizeof(readBuffer_),
                                         &bytesRead, pdMS_TO_TICKS(80));
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) return true;  // fatal -> stop
  if (bytesRead == 0) return false;

  const size_t remaining = maxAudioBytes() - audioBytes_;
  const size_t toCopy = (bytesRead < remaining) ? bytesRead : remaining;
  memcpy(wavBuffer_ + kWavHeaderBytes + audioBytes_, readBuffer_, toCopy);
  audioBytes_ += toCopy;

  return toCopy < bytesRead || audioBytes_ >= maxAudioBytes();
}

void AudioCapture::finishRecord()
{
  writeWavHeader(audioBytes_);
}
