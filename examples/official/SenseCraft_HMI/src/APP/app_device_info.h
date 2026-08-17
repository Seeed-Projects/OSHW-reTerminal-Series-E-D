#pragma once

#include <Arduino.h>
#include <esp_err.h>
#include <vector>
#include <utility>

void DeviceInfoEarlyInit();
bool DeviceInfoInit();

enum class AppContentMode : uint8_t
{
    Dashboard,
    Gallery,
};

AppContentMode GetContentMode();
esp_err_t SetContentMode(AppContentMode mode);
bool IsGalleryContent();

bool IsLocalGallerySession();
esp_err_t SetLocalGallerySession(bool enabled);

String GetContentVersion();
esp_err_t SetContentVersion(const String &version);
int GetImageCount();
esp_err_t SetImageCount(int count);
bool HasImage();
esp_err_t SetHasImage(bool has_image);
String GetCurrentImageId();
esp_err_t SetCurrentImageId(const String &id);
int GetCurrentImageOrder();
esp_err_t SetCurrentImageOrder(int order);
String GetAlbumVersion();
esp_err_t SetAlbumVersion(const String &version);
uint32_t GetDeepSleepInterval();
esp_err_t SetDeepSleepInterval(uint32_t seconds);
bool GetDeepSleepEnabled();
esp_err_t SetDeepSleepEnabled(bool enabled);
esp_err_t SetCloudToken(const String &token);
String GetCloudToken();
bool GetBindState();
esp_err_t SetBindState(bool bound);
esp_err_t SetActivationCode(int code);
int GetActivationCode();

bool IsTimerWakeup();
esp_err_t SetTimerWakeup(bool enabled);
void MarkUserWakeup();

void EnterDeepSleep();

void ResetAfterUnbind();

bool SaveWifiCredential(const String &ssid, const String &password);
bool GetWifiCredentials(std::vector<std::pair<String, String>> &credentials);
bool GetWifiPassword(const String &ssid, String &password);
bool RemoveWifiCredential(const String &ssid);

void StartDeepSleepTimer();
void StopDeepSleepTimer();
void CancelPendingWakeupAction();
