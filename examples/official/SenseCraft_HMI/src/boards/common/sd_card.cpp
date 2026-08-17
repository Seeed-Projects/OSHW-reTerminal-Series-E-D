#include "boards/common/sd_card.h"

#include <errno.h>
#include <cstring>

#include <SD.h>

#include "ArduinoLog.h"
#include "TFT_eSPI.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {
constexpr int SD_INIT_ATTEMPTS = 3;
constexpr int SD_INIT_RETRY_DELAY_MS = 200;
constexpr int SD_POWER_OFF_DELAY_MS = 120;
constexpr int SD_POWER_ON_DELAY_MS = 300;
constexpr int SD_SPI_SETTLE_DELAY_MS = 20;
constexpr int SD_WRITE_SETTLE_DELAY_MS = 30;
constexpr uint32_t SD_INIT_FREQUENCIES[] = {1000000, 2000000, 4000000};
constexpr const char* SD_WRITE_PROBE_PATH = "/.sd_write_test.tmp";
constexpr size_t SD_WRITE_PROBE_SIZE = 4096;
constexpr size_t SD_WRITE_PROBE_CHUNK = 512;

SemaphoreHandle_t sharedBusMutex()
{
    static SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
    return mutex;
}

const char* cardTypeName(sdcard_type_t type)
{
    switch (type) {
    case CARD_MMC:
        return "MMC";
    case CARD_SD:
        return "SDSC";
    case CARD_SDHC:
        return "SDHC/SDXC";
    case CARD_NONE:
        return "none";
    case CARD_UNKNOWN:
    default:
        return "unknown";
    }
}

class SharedBusGuard {
public:
    SharedBusGuard()
    {
        SdCard::lockSharedBus();
    }

    ~SharedBusGuard()
    {
        SdCard::unlockSharedBus();
    }
};
}

SdCard::SdCard(const Config& config)
    : config_(config), owned_spi_(HSPI), spi_(&owned_spi_)
{
}

void SdCard::initPins()
{
    if (config_.det_pin >= 0) {
        pinMode(config_.det_pin, INPUT_PULLUP);
    }

    if (config_.miso_pin >= 0) {
        gpio_set_pull_mode(static_cast<gpio_num_t>(config_.miso_pin), GPIO_PULLUP_ONLY);
    }
    if (config_.mosi_pin >= 0) {
        gpio_set_pull_mode(static_cast<gpio_num_t>(config_.mosi_pin), GPIO_PULLUP_ONLY);
    }
    if (config_.sck_pin >= 0) {
        gpio_set_pull_mode(static_cast<gpio_num_t>(config_.sck_pin), GPIO_PULLUP_ONLY);
    }
    if (config_.cs_pin >= 0) {
        gpio_set_pull_mode(static_cast<gpio_num_t>(config_.cs_pin), GPIO_PULLUP_ONLY);
    }

    setChipSelectsInactive();
    powerOn();
}

void SdCard::setChipSelectsInactive()
{
    if (config_.cs_pin >= 0) {
        pinMode(config_.cs_pin, OUTPUT);
        digitalWrite(config_.cs_pin, HIGH);
    }

#if defined(TFT_CS) && (TFT_CS >= 0)
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
#endif

#if defined(TFT_CS1) && (TFT_CS1 >= 0)
    pinMode(TFT_CS1, OUTPUT);
    digitalWrite(TFT_CS1, HIGH);
#endif
}

void SdCard::powerOn()
{
    if (config_.en_pin >= 0) {
        pinMode(config_.en_pin, OUTPUT);
        digitalWrite(config_.en_pin, HIGH);
        delay(SD_POWER_ON_DELAY_MS);
    }
}

void SdCard::powerCycle()
{
    setChipSelectsInactive();

    if (config_.en_pin < 0) {
        delay(SD_SPI_SETTLE_DELAY_MS);
        return;
    }

    pinMode(config_.en_pin, OUTPUT);
    digitalWrite(config_.en_pin, LOW);
    delay(SD_POWER_OFF_DELAY_MS);
    digitalWrite(config_.en_pin, HIGH);
    delay(SD_POWER_ON_DELAY_MS);
    setChipSelectsInactive();
}

void SdCard::useSpi(SPIClass& spi)
{
    spi_ = &spi;
}

void SdCard::releaseBus()
{
    setChipSelectsInactive();
}

bool SdCard::init()
{
    SharedBusGuard lock;
    initPins();

    if (config_.det_pin >= 0 && !isInserted()) {
        mounted_ = false;
        writable_ = false;
        return false;
    }

    if (mounted_ && isReady()) {
        return true;
    }

    if (mounted_) {
        SD.end();
        mounted_ = false;
        writable_ = false;
        delay(SD_SPI_SETTLE_DELAY_MS);
    }

    powerCycle();

    for (uint32_t frequency : SD_INIT_FREQUENCIES) {
        for (int attempt = 1; attempt <= SD_INIT_ATTEMPTS; ++attempt) {
            setChipSelectsInactive();
            SD.end();
            spi_->end();
            delay(SD_SPI_SETTLE_DELAY_MS);
            spi_->begin(config_.sck_pin, config_.miso_pin, config_.mosi_pin, -1);
            setChipSelectsInactive();

            if (SD.begin(config_.cs_pin, *spi_, frequency)) {
                mounted_ = true;
                writable_ = false;

                if (!isReadable()) {
                    SD.end();
                    mounted_ = false;
                    Log.warningln("[Board] MicroSD init attempt %d/%d at %luHz could not open root directory.",
                                  attempt,
                                  SD_INIT_ATTEMPTS,
                                  static_cast<unsigned long>(frequency));
                }
                else if (!probeWriteAccess()) {
                    SD.end();
                    mounted_ = false;
                    Log.warningln("[Board] MicroSD init attempt %d/%d at %luHz failed write probe.",
                                  attempt,
                                  SD_INIT_ATTEMPTS,
                                  static_cast<unsigned long>(frequency));
                }
                else {
                    writable_ = true;
                    logCardInfo(frequency);
                    Log.infoln("[Board] MicroSD found and initialization succeed.");
                    return true;
                }
            }
            else {
                Log.warningln("[Board] MicroSD init attempt %d/%d at %luHz failed.",
                              attempt,
                              SD_INIT_ATTEMPTS,
                              static_cast<unsigned long>(frequency));
            }

            mounted_ = false;
            writable_ = false;
            powerCycle();
            if (attempt < SD_INIT_ATTEMPTS) {
                delay(SD_INIT_RETRY_DELAY_MS);
            }
        }
    }

    Log.errorln("[Board] MicroSD initialization failed");
    return false;
}

void SdCard::deinit()
{
    SharedBusGuard lock;
    if (!mounted_) {
        return;
    }

    SD.end();
    mounted_ = false;
    writable_ = false;

    if (config_.en_pin >= 0) {
        digitalWrite(config_.en_pin, LOW);
        pinMode(config_.en_pin, INPUT);
    }

    Log.infoln("[Board] MicroSD deinitialized.");
}

bool SdCard::isInserted() const
{
    if (config_.det_pin >= 0) {
        return digitalRead(config_.det_pin) == LOW;
    }

    return mounted_;
}

bool SdCard::supportsHotplugDetection() const
{
    return config_.det_pin >= 0;
}

bool SdCard::isMounted() const
{
    return mounted_;
}

bool SdCard::isReady() const
{
    SharedBusGuard lock;
    return writable_ && isReadable();
}

bool SdCard::ensureReady()
{
    SharedBusGuard lock;
    if (isReady()) {
        return true;
    }

    if (config_.det_pin >= 0 && !isInserted()) {
        if (mounted_) {
            deinit();
        }
        return false;
    }

    if (mounted_) {
        Log.warningln("[Board] MicroSD mounted but not readable/writable, reinitializing.");
    }

    return init();
}

fs::File SdCard::open(const char* path, const char* mode)
{
    SharedBusGuard lock;
    if (!ensureReady()) {
        return fs::File();
    }

    return SD.open(path, mode);
}

bool SdCard::exists(const char* path)
{
    SharedBusGuard lock;
    return isReady() && SD.exists(path);
}

bool SdCard::remove(const char* path)
{
    SharedBusGuard lock;
    return isReady() && SD.remove(path);
}

uint64_t SdCard::totalBytes() const
{
    SharedBusGuard lock;
    return mounted_ ? SD.totalBytes() : 0;
}

uint64_t SdCard::usedBytes() const
{
    SharedBusGuard lock;
    return mounted_ ? SD.usedBytes() : 0;
}

uint64_t SdCard::cardSize() const
{
    SharedBusGuard lock;
    return mounted_ ? SD.cardSize() : 0;
}

bool SdCard::isReadable() const
{
    if (!mounted_) {
        return false;
    }

    if (config_.det_pin >= 0 && !isInserted()) {
        return false;
    }

    File root = SD.open("/");
    if (!root) {
        return false;
    }

    bool ready = root.isDirectory();
    root.close();
    return ready;
}

bool SdCard::probeWriteAccess()
{
    if (SD.totalBytes() == 0) {
        Log.warningln("[Board] MicroSD mounted but filesystem size is 0. Reformat as FAT32 if this is an SDXC card.");
        return false;
    }

    SD.remove(SD_WRITE_PROBE_PATH);

    errno = 0;
    File file = SD.open(SD_WRITE_PROBE_PATH, FILE_WRITE);
    if (!file) {
        Log.warningln("[Board] MicroSD write probe open failed. errno=%d", errno);
        return false;
    }

    uint8_t buffer[SD_WRITE_PROBE_CHUNK];
    for (size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = static_cast<uint8_t>((i * 31U) + 0x5aU);
    }

    errno = 0;
    size_t written = 0;
    while (written < SD_WRITE_PROBE_SIZE) {
        size_t toWrite = SD_WRITE_PROBE_SIZE - written;
        if (toWrite > sizeof(buffer)) {
            toWrite = sizeof(buffer);
        }

        size_t chunk = file.write(buffer, toWrite);
        if (chunk != toWrite) {
            Log.warningln("[Board] MicroSD write probe failed. expected=%u, wrote=%u, errno=%d",
                          static_cast<unsigned>(toWrite),
                          static_cast<unsigned>(chunk),
                          errno);
            file.close();
            SD.remove(SD_WRITE_PROBE_PATH);
            return false;
        }
        written += chunk;
    }

    file.flush();
    file.close();
    delay(SD_WRITE_SETTLE_DELAY_MS);

    errno = 0;
    file = SD.open(SD_WRITE_PROBE_PATH, FILE_READ);
    if (!file) {
        Log.warningln("[Board] MicroSD write probe reopen failed. errno=%d", errno);
        SD.remove(SD_WRITE_PROBE_PATH);
        return false;
    }

    size_t verified = 0;
    uint8_t readBuffer[SD_WRITE_PROBE_CHUNK];
    while (verified < SD_WRITE_PROBE_SIZE) {
        size_t toRead = SD_WRITE_PROBE_SIZE - verified;
        if (toRead > sizeof(readBuffer)) {
            toRead = sizeof(readBuffer);
        }

        errno = 0;
        size_t chunk = file.read(readBuffer, toRead);
        if (chunk != toRead || memcmp(readBuffer, buffer, toRead) != 0) {
            Log.warningln("[Board] MicroSD write probe verify failed. expected=%u, read=%u, errno=%d",
                          static_cast<unsigned>(toRead),
                          static_cast<unsigned>(chunk),
                          errno);
            file.close();
            SD.remove(SD_WRITE_PROBE_PATH);
            return false;
        }
        verified += chunk;
    }
    file.close();

    if (!SD.remove(SD_WRITE_PROBE_PATH)) {
        Log.warningln("[Board] MicroSD write probe cleanup failed.");
        return false;
    }

    delay(SD_WRITE_SETTLE_DELAY_MS);
    return isReadable();
}

void SdCard::logCardInfo(uint32_t frequency) const
{
    uint32_t cardMb = static_cast<uint32_t>(SD.cardSize() / (1024ULL * 1024ULL));
    uint32_t totalMb = static_cast<uint32_t>(SD.totalBytes() / (1024ULL * 1024ULL));
    uint32_t usedMb = static_cast<uint32_t>(SD.usedBytes() / (1024ULL * 1024ULL));
    Log.infoln("[Board] MicroSD info: type=%s, card=%luMB, fs_total=%luMB, fs_used=%luMB, spi=%luHz",
               cardTypeName(SD.cardType()),
               static_cast<unsigned long>(cardMb),
               static_cast<unsigned long>(totalMb),
               static_cast<unsigned long>(usedMb),
               static_cast<unsigned long>(frequency));
}

void SdCard::lockSharedBus()
{
    SemaphoreHandle_t mutex = sharedBusMutex();
    if (mutex) {
        xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
    }
}

void SdCard::unlockSharedBus()
{
    SemaphoreHandle_t mutex = sharedBusMutex();
    if (mutex) {
        xSemaphoreGiveRecursive(mutex);
    }
}
