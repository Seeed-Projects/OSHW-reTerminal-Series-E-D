// app_ble.cpp
#include "APP/app_ble.h"
#include "APP/app_wifi.h"
#include "hal/hal.h"

#include "app_config.h"

#include "utils/util.h"

#include "minirt.h"
#include "APP/app_events.h"

#include "esp_gap_ble_api.h"
#include "esp_heap_caps.h"

#include "ArduinoLog.h"
#include "cJSON.h"
#include <string>

#define SERVICE_UUID "49535343-fe7d-4ae5-8fa9-9fafd205e455"
#define WRITE_CHR_UUID "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHR_UUID "49535343-1e4d-4bd9-ba61-23c647249616"

// battery service UUID
#define BATTERY_SERVICE_UUID (uint16_t)0x180F
#define BATTERY_LEVEL_CHR_UUID (uint16_t)0x2A19

namespace
{
    void ble_monitor_task()
    {
        AppBLE::getInstance().loop();
    }

    bool g_ble_app_registered = false;
}

AppBLE &AppBLE::getInstance()
{
    static AppBLE instance;
    return instance;
}

AppBLE::AppBLE()
{
    m_indicateSemaphore = xSemaphoreCreateBinary();
    m_sendMutex = xSemaphoreCreateMutex();
    if (m_indicateSemaphore == nullptr || m_sendMutex == nullptr)
    {
        Log.errorln("[app_ble] Failed to create sync objects!");
    }

    m_pServerCallbacks = new ServerCallbacks(this);
    m_pCharCallbacks = new CharacteristicCallbacks(this);
    if (m_pServerCallbacks == nullptr || m_pCharCallbacks == nullptr)
    {
        Log.errorln("[app_ble] Failed to create callback objects!");
    }
}

void AppBLE::init(const std::string &deviceName)
{
    if (m_isInitialized)
    {
        Log.warningln("[app_ble] Already initialized, skipping init.");
        return;
    }

    NimBLEDevice::init("");
    NimBLEDevice::setSecurityAuth(false, false, false);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(m_pServerCallbacks);

    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    pWriteCharacteristic = pService->createCharacteristic(
        WRITE_CHR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
    pWriteCharacteristic->setCallbacks(m_pCharCallbacks);

    pReadCharacteristic = pService->createCharacteristic(
        READ_CHR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE);
    pReadCharacteristic->setCallbacks(m_pCharCallbacks);
    pService->start();

    NimBLEService *pBatteryService = pServer->createService(BATTERY_SERVICE_UUID);

    pBatteryLevelCharacteristic = pBatteryService->createCharacteristic(
        BATTERY_LEVEL_CHR_UUID,
        NIMBLE_PROPERTY::READ);

    uint8_t initial_battery_level = HAL::GetHAL().batteryReadPercent();
    pBatteryLevelCharacteristic->setValue(initial_battery_level);

    pBatteryService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData advData;

    std::string ble_name = HAL::GetHAL().bleShortName();

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[13];
    snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ble_name += "-";
    ble_name += mac_str;

    advData.setName(ble_name);
    advData.addServiceUUID((uint16_t)0x2886);
    advData.addServiceUUID((uint16_t)0xA886);
    advData.setFlags(0x06);
    pAdvertising->setAdvertisementData(advData);

    // pAdvertising->addServiceUUID(SERVICE_UUID);
    // pAdvertising->addServiceUUID(BATTERY_SERVICE_UUID);
    // NimBLEDevice::startAdvertising();

    m_isInitialized = true;
    Log.infoln("[app_ble] BLE module initialized");
}

void AppBLE::deinit()
{
    if (!m_isInitialized)
    {
        Log.warningln("[app_ble] Not initialized, skipping deinit.");
        return;
    }
    Log.verboseln("[app_ble] de-initializing BLE");

    if (NimBLEDevice::getAdvertising()->isAdvertising())
    {
        NimBLEDevice::getAdvertising()->stop();
    }

    if (this->isConnected() && pServer != nullptr)
    {
        pServer->disconnect(m_connHandle);
    }

    if (NimBLEDevice::isInitialized())
    {
        NimBLEDevice::deinit();
    }

    this->deviceConnected = false;
    this->oldDeviceConnected = false;
    this->m_connHandle = 0;
    this->pServer = nullptr;
    this->pWriteCharacteristic = nullptr;
    this->pReadCharacteristic = nullptr;
    this->pBatteryLevelCharacteristic = nullptr;

    m_isInitialized = false;
    Log.verboseln("[app_ble] BLE module de-initialized");
}

void AppBLE::setBatteryLevel(uint8_t level)
{
    if (this->pBatteryLevelCharacteristic != nullptr)
    {
        if (level > 100)
            level = 100;
        this->pBatteryLevelCharacteristic->setValue(level);
        Log.verboseln("[app_ble] battery level updated to: %u%%", level);
    }
}

bool AppBLE::sendExternalMessage(const std::string &message)
{
    if (this->isConnected())
    {
        Log.verboseln("[app_ble] sending external message: %s", message.c_str());
        return this->sendData(message);
    }

    Log.warningln("[app_ble] Cannot send external message, BLE not connected.");
    return false;
}

void AppBLE::loop()
{
    if (!deviceConnected && oldDeviceConnected)
    {
        Log.verboseln("[app_ble] client disconnected; ready for new connection when advertising restarts");
        oldDeviceConnected = deviceConnected;
        if (get_portal_status() || app_wifi_get_origin() == ProvisioningOrigin::BleAt)
            app_ble_start_advertising();
    }
    if (deviceConnected && !oldDeviceConnected)
    {
        oldDeviceConnected = deviceConnected;
    }
}

bool AppBLE::sendData(const uint8_t *data, size_t length)
{
    if (xSemaphoreTake(m_sendMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        Log.errorln("[app_ble] Could not get send mutex. Function busy.");
        return false;
    }

    if (!this->isConnected() || this->pReadCharacteristic == nullptr || this->pServer == nullptr || this->m_connHandle == 0)
    {
        Log.warningln("[app_ble] Cannot send data, not connected or characteristic not available.");
        xSemaphoreGive(m_sendMutex);
        return false;
    }

    xSemaphoreTake(m_indicateSemaphore, 0);

    uint16_t mtu = this->pServer->getPeerMTU(this->m_connHandle);
    size_t maxPacketSize = mtu > 3 ? (mtu - 3) : 20;
    size_t offset = 0;
    bool success = true;

    while (offset < length)
    {
        size_t chunkSize = std::min(length - offset, maxPacketSize);

        this->pReadCharacteristic->indicate(data + offset, chunkSize);

        if (xSemaphoreTake(m_indicateSemaphore, pdMS_TO_TICKS(5000)) != pdTRUE)
        {
            Log.errorln("[app_ble] Timed out waiting for indication confirmation!");
            success = false;
            break;
        }

        Log.verboseln("[app_ble] confirmed sent: chunk size %u / total %u", chunkSize, length);
        offset += chunkSize;
    }

    xSemaphoreGive(m_sendMutex);
    return success;
}

bool AppBLE::sendData(const std::string &data)
{
    return sendData(reinterpret_cast<const uint8_t *>(data.c_str()), data.length());
}

bool AppBLE::isConnected() const
{
    return this->deviceConnected;
}

void AppBLE::disconnectClient()
{
    if (this->isConnected() && this->pServer != nullptr && this->m_connHandle != 0)
    {
        Log.verboseln("[app_ble] actively disconnecting client handle %u", this->m_connHandle);
        this->pServer->disconnect(this->m_connHandle);
    }
}

void AppBLE::setRxCallback(std::function<void(const std::string &)> cb)
{
    this->rxCallback = cb;
}

void AppBLE::ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo)
{
    Log.infoln("[app_ble] Client Connected! Conn Handle: %u", connInfo.getConnHandle());
    m_pAppBle->deviceConnected = true;
    m_pAppBle->m_connHandle = connInfo.getConnHandle();

    // esp_event_post(WIFI_APP_EVENT_BASE, WIFI_PORTAL_STOP, nullptr, 0, portMAX_DELAY);
}

void AppBLE::ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason)
{
    Log.infoln("[app_ble] Client Disconnected! Reason: %d", reason);
    m_pAppBle->deviceConnected = false;
    m_pAppBle->m_connHandle = 0;
}

void AppBLE::CharacteristicCallbacks::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0)
    {
        // if (m_pAppBle->rxCallback) {
        //     m_pAppBle->rxCallback(rxValue);
        // } else {
        //     Log.infoln("[app_ble] Received Value: %s", rxValue.c_str());
        // }
        Log.verboseln("[app_ble] received data: '%s', posting to ATCMD loop", rxValue.c_str());
        esp_event_post(CTRL_EVENT_BASE, APP_ATCMD_BLE_RX, (void *)rxValue.c_str(), rxValue.length() + 1, portMAX_DELAY);
    }
}

void AppBLE::CharacteristicCallbacks::onStatus(NimBLECharacteristic *pCharacteristic, int code)
{
    xSemaphoreGive(m_pAppBle->m_indicateSemaphore);
    Log.verboseln("[app_ble] indication complete, status: %d", code);
}

void app_ble_init()
{
    if (g_ble_app_registered)
    {
        Log.verboseln("[app_ble] BLE is already initialized.");
        return;
    }

    AppBLE::getInstance().init("reTerminal");

    // AppBLE::getInstance().setRxCallback(onBleReceived);

    if (!MiniRT::addTask(ble_monitor_task, 10, MiniRT::Priority::Low))
    {
        Log.errorln("[app_ble] Failed to register BLE monitor task with MiniRT");
        return;
    }

    g_ble_app_registered = true;
}

void app_ble_deinit()
{
    if (g_ble_app_registered)
    {
        if (!MiniRT::removeTask(ble_monitor_task, MiniRT::Priority::Low))
        {
            Log.warningln("[app_ble] Failed to remove BLE monitor task from MiniRT");
        }
        else
        {
            Log.verboseln("[app_ble] BLE monitor task stopped");
        }
        g_ble_app_registered = false;
    }

    AppBLE::getInstance().deinit();
}

void app_ble_start_advertising()
{
    if (NimBLEDevice::getAdvertising())
    {
        if (!NimBLEDevice::getAdvertising()->isAdvertising())
        {
            Log.verboseln("[app_ble] Starting BLE advertising...");
            NimBLEDevice::getAdvertising()->start();
        }
        else
        {
            Log.verboseln("[app_ble] Advertising is already active.");
        }
    }
    else
    {
        Log.errorln("[app_ble] Cannot start advertising, BLE not initialized.");
    }
}

void app_ble_stop_advertising()
{
    AppBLE::getInstance().disconnectClient();

    if (NimBLEDevice::getAdvertising())
    {
        if (NimBLEDevice::getAdvertising()->isAdvertising())
        {
            Log.verboseln("[app_ble] Stopping BLE advertising...");
            NimBLEDevice::getAdvertising()->stop();
        }
        else
        {
            Log.verboseln("[app_ble] Advertising is already stopped.");
        }
    }
    else
    {
        Log.verboseln("[app_ble] Cannot stop advertising, BLE may not be initialized.");
    }
}

bool app_ble_send_message_external(const std::string &message)
{
    return AppBLE::getInstance().sendExternalMessage(message);
}
