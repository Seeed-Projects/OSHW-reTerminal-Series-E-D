// app_ble.h
#pragma once

#include <NimBLEDevice.h>
#include <functional>

class AppBLE {
public:
    static AppBLE& getInstance();

    AppBLE(const AppBLE&) = delete;
    AppBLE& operator=(const AppBLE&) = delete;

    void init(const std::string& deviceName);
    void deinit();

    void loop();

    bool sendData(const uint8_t* data, size_t length);
    bool sendData(const std::string& data);

    bool isConnected() const;
    void disconnectClient();

    void setRxCallback(std::function<void(const std::string&)> cb);

    void setBatteryLevel(uint8_t level);

    bool sendExternalMessage(const std::string& message);

private:
    NimBLEServer* pServer = nullptr;
    NimBLECharacteristic* pWriteCharacteristic = nullptr;
    NimBLECharacteristic* pReadCharacteristic = nullptr;

    NimBLECharacteristic* pBatteryLevelCharacteristic = nullptr; 
    
    bool deviceConnected = false;
    bool oldDeviceConnected = false;
    uint16_t m_connHandle = 0;
    SemaphoreHandle_t m_indicateSemaphore = nullptr;
    SemaphoreHandle_t m_sendMutex = nullptr;

    std::function<void(const std::string&)> rxCallback;

    bool m_isInitialized = false;
    class ServerCallbacks;
    class CharacteristicCallbacks;

    ServerCallbacks* m_pServerCallbacks = nullptr;
    CharacteristicCallbacks* m_pCharCallbacks = nullptr;

    AppBLE(); 

    class ServerCallbacks : public NimBLEServerCallbacks {
    public:
        ServerCallbacks(AppBLE* pAppBle) : m_pAppBle(pAppBle) {}
        void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
        void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    private:
        AppBLE* m_pAppBle;
    };

    class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    public:
        CharacteristicCallbacks(AppBLE* pAppBle) : m_pAppBle(pAppBle) {}
        void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
        void onStatus(NimBLECharacteristic* pCharacteristic, int code) override;
    private:
        AppBLE* m_pAppBle;
    };
};

void app_ble_init();
void app_ble_deinit();

void app_ble_start_advertising();
void app_ble_stop_advertising();

bool app_ble_send_message_external(const std::string& message);
