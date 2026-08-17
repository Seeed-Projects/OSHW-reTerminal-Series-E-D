#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>

class SdCard {
public:
    struct Config {
        int sck_pin;
        int miso_pin;
        int mosi_pin;
        int cs_pin;
        int en_pin;
        int det_pin;
    };

    explicit SdCard(const Config& config);

    void initPins();
    void useSpi(SPIClass& spi);
    void releaseBus();
    bool init();
    void deinit();
    bool isInserted() const;
    bool supportsHotplugDetection() const;
    bool isMounted() const;
    bool isReady() const;
    bool ensureReady();
    fs::File open(const char* path, const char* mode = FILE_READ);
    bool exists(const char* path);
    bool remove(const char* path);
    uint64_t totalBytes() const;
    uint64_t usedBytes() const;
    uint64_t cardSize() const;

    static void lockSharedBus();
    static void unlockSharedBus();

private:
    void setChipSelectsInactive();
    void powerOn();
    void powerCycle();
    bool isReadable() const;
    bool probeWriteAccess();
    void logCardInfo(uint32_t frequency) const;

    Config config_;
    SPIClass owned_spi_;
    SPIClass* spi_;
    bool mounted_ = false;
    bool writable_ = false;
};
