#include "APP/app_download.h"
#include "APP/app_sensecraft.h"
#include "APP/app_device_info.h"
#include "APP/app_gallery.h"
#include "APP/app_power_manager.h"
#include "APP/app_view.h"

#include "hal/hal_indicator.h"
#include "utils/storage.h"
#include "utils/mem_malloc.h"

#include <Arduino.h>

#include <atomic>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <limits>
#include <cstring>
#include <cstdlib>
#include <errno.h>

#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "mbedtls/md5.h"
#include "ArduinoLog.h"
#include "cJSON.h"

#include "app_config.h"
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "hal/hal.h"

#include "APP/app_events.h"
#include "esp_heap_caps.h"

constexpr int HTTP_TIMEOUT_MS = 10000;
constexpr uint32_t DOWNLOAD_IDLE_TIMEOUT_MS = 15000;
constexpr int MAX_DOWNLOAD_RETRIES = 3;
constexpr int MAX_EMPTY_MANIFEST_RETRIES = 3;
constexpr uint32_t DOWNLOAD_TASK_STACK_SIZE = 8192;
constexpr UBaseType_t DOWNLOAD_TASK_PRIORITY = tskIDLE_PRIORITY + 1;
constexpr TickType_t DOWNLOAD_TASK_INTERVAL = pdMS_TO_TICKS(10);

constexpr size_t DOWNLOAD_WRITE_CHUNK_SIZE = 4096;
constexpr int FS_WRITE_MAX_RETRIES = 5;
constexpr int FS_WRITE_RETRY_DELAY_MS = 50;

enum class DownloadResult
{
    Success = 0,
    HttpBeginFailed = -1,
    FileOpenFailed = -2,
    WriteFailed = -3,
    PsramAllocFailed = -4,
    ContentLengthInvalid = -5,
    ImageTooLarge = -6,
    Md5Mismatch = -7,
    DownloadIncomplete = -8,
    ImageFormatUnknown = -9,
    ManifestParseFailed = -10,
    ManifestImagesMissing = -11,
    ManifestReadFailed = -12,
    ManifestEmpty = -13,
    ManifestEntriesInvalid = -14,
};

enum class ResourceKind
{
    StaticImage,
    DynamicCanvas,
    UnknownDynamic,
};

enum class ImageFormat
{
    Unknown,
    BMP,
    PNG,
    EPD,
};

enum class ImageTaskType
{
    Reuse,
    Download,
};

struct ManifestImage
{
    String id;
    String url;
    int order;
    ResourceKind kind;
};

struct LocalImage
{
    String id;
    String path;
    bool onSd;
    ImageFormat format;
};

struct ImageTask
{
    ImageTaskType type;
    String id;
    String url;
    String path;
    int order;
    bool storeOnSd;
    ImageFormat format;
    ResourceKind kind;
};

ESP_EVENT_DEFINE_BASE(DOWNLOAD_EVENT_BASE);

static std::vector<ImageTask, util::psram_allocator<ImageTask>> s_imageTasks;
static int s_currentTaskIndex = 0;
static int s_totalImagesInManifest = 0;
static ctrl_image_refresh s_imageRefreshData;
static bool use_sdcard = false;

char *g_psramManifestBuffer = nullptr;
size_t g_psramManifestSize = 0;

static uint8_t *s_downloadPsramBuffer = nullptr;
static size_t s_downloadPsramCapacity = 0;
static size_t s_downloadPsramSize = 0;

static EventGroupHandle_t __download_event_group;
const int DL_EVT_JSON_DOWNLOAD = BIT0;
const int DL_EVT_JSON_PARSE = BIT1;
const int DL_EVT_IMAGE_DOWNLOAD = BIT2;
const int DL_EVT_TASK_FINISHED = BIT3;
const int DL_EVT_TASK_CANCEL = BIT4;

static String s_manifestUrl;
static String s_manifestVersion;
static String s_batchVersion;
static String s_onlyId;
static bool _new_task = false;
static std::atomic<bool> s_download_power_held{false};
static SemaphoreHandle_t s_download_state_mutex = xSemaphoreCreateMutex();
static bool s_download_active = false;

static void app_download_set_active(bool active)
{
    xSemaphoreTake(s_download_state_mutex, portMAX_DELAY);
    s_download_active = active;
    xSemaphoreGive(s_download_state_mutex);
}

static void app_download_acquire_power()
{
    bool expected = false;
    if (s_download_power_held.compare_exchange_strong(expected, true))
    {
        app_power_manager_acquire(AppPowerOwner::Download);
    }
}

static void app_download_release_power()
{
    bool expected = true;
    if (s_download_power_held.compare_exchange_strong(expected, false))
    {
        app_power_manager_release(AppPowerOwner::Download);
    }
}

static String app_download_manifest_url()
{
    xSemaphoreTake(s_download_state_mutex, portMAX_DELAY);
    String url = s_manifestUrl;
    xSemaphoreGive(s_download_state_mutex);
    return url;
}

static String app_download_only_id()
{
    xSemaphoreTake(s_download_state_mutex, portMAX_DELAY);
    String id = s_onlyId;
    xSemaphoreGive(s_download_state_mutex);
    return id;
}

static bool __has_cached_image_for_fallback()
{
    return HasImage() && GetImageCount() > 0 && app_gallery_has_playable();
}

static void __fallback_to_cached_image_after_failure()
{
    app_download_set_active(false);
    app_download_release_power();

    if (!__has_cached_image_for_fallback())
    {
        Log.warningln("[app_download] No cached image available after download failure.");
        return;
    }

    SetLocalGallerySession(IsGalleryContent());

    Log.warningln("[app_download] Download failed, showing cached local image.");
    esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, NULL, 1, portMAX_DELAY);
}

static bool ensure_download_buffer(size_t size)
{
    if (size == 0)
    {
        if (s_downloadPsramBuffer)
        {
            heap_caps_free(s_downloadPsramBuffer);
            s_downloadPsramBuffer = nullptr;
        }
        s_downloadPsramCapacity = 0;
        s_downloadPsramSize = 0;
        return true;
    }

    if (s_downloadPsramBuffer != nullptr && s_downloadPsramCapacity >= size)
    {
        s_downloadPsramSize = size;
        return true;
    }

    if (s_downloadPsramBuffer)
    {
        heap_caps_free(s_downloadPsramBuffer);
        s_downloadPsramBuffer = nullptr;
        s_downloadPsramCapacity = 0;
    }

    s_downloadPsramBuffer = static_cast<uint8_t *>(ps_malloc(size));
    if (!s_downloadPsramBuffer)
    {
        Log.errorln("[app_download] Failed to allocate download buffer of %u bytes", static_cast<unsigned>(size));
        s_downloadPsramSize = 0;
        return false;
    }

    s_downloadPsramCapacity = size;
    s_downloadPsramSize = size;
    return true;
}

static ResourceKind __resource_kind_from_url(const String &url)
{
    int render_pos = url.indexOf("/render/");
    if (render_pos < 0)
    {
        return ResourceKind::UnknownDynamic;
    }

    int type_start = render_pos + 8;
    int type_end = url.indexOf('/', type_start);
    String type = type_end > type_start ? url.substring(type_start, type_end) : url.substring(type_start);

    if (type == "img")
    {
        return ResourceKind::StaticImage;
    }
    if (type == "layout")
    {
        return ResourceKind::DynamicCanvas;
    }

    return ResourceKind::UnknownDynamic;
}

static ImageFormat __image_format_from_name(const String &name)
{
    if (name.endsWith(".bmp"))
    {
        return ImageFormat::BMP;
    }
    if (name.endsWith(".png"))
    {
        return ImageFormat::PNG;
    }
    if (name.endsWith(".epd"))
    {
        return ImageFormat::EPD;
    }
    return ImageFormat::Unknown;
}

static ImageFormat __image_format_from_buffer(const uint8_t *data, size_t len)
{
    const uint8_t png_sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (len >= 2 && data[0] == 'B' && data[1] == 'M')
    {
        return ImageFormat::BMP;
    }
    if (len >= sizeof(png_sig) && memcmp(data, png_sig, sizeof(png_sig)) == 0)
    {
        return ImageFormat::PNG;
    }
    if (len >= 4 && data[0] == 'E' && data[1] == 'P' && data[2] == 'D' && data[3] == '0')
    {
        return ImageFormat::EPD;
    }
    return ImageFormat::Unknown;
}

static const char *__image_ext(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::BMP:
        return ".bmp";
    case ImageFormat::PNG:
        return ".png";
    case ImageFormat::EPD:
        return ".epd";
    default:
        return "";
    }
}

static String __image_path(const String &id, ImageFormat format)
{
    return "/" + id + __image_ext(format);
}

static bool __is_image_file(const String &name)
{
    return __image_format_from_name(name) != ImageFormat::Unknown;
}

static bool __can_reuse_manifest_image(const ManifestImage &image)
{
    return image.kind == ResourceKind::StaticImage;
}

static void __remove_other_image_variants(const String &id, const String &keepPath, bool keepOnSd)
{
    const ImageFormat keepFormat = __image_format_from_name(keepPath);
    const ImageFormat formats[] = {ImageFormat::EPD, ImageFormat::BMP, ImageFormat::PNG};

    for (ImageFormat format : formats)
    {
        String path = __image_path(id, format);
        if (keepOnSd || format != keepFormat)
        {
            bool removed = LittleFS.remove(path.c_str());
            if (removed)
            {
                Log.verboseln("[app_download] Removed stale LittleFS image variant: %s", path.c_str());
            }
        }

        HAL::SharedSpiLock sdLock;
        if (HAL::GetHAL().sdIsReady() && (!keepOnSd || format != keepFormat))
        {
            bool removed = HAL::GetHAL().sdRemove(path.c_str());
            if (removed)
            {
                Log.verboseln("[app_download] Removed stale MicroSD image variant: %s", path.c_str());
            }
        }
    }
}

static bool __local_image_header_valid(File &file, ImageFormat format)
{
    if (file.size() < 4 || !file.seek(0))
    {
        return false;
    }

    uint8_t header[8] = {};
    size_t headerLength = file.read(header, sizeof(header));
    if (format == ImageFormat::BMP)
    {
        return headerLength >= 2 && header[0] == 'B' && header[1] == 'M';
    }
    if (format == ImageFormat::PNG)
    {
        const uint8_t png_sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        return headerLength >= sizeof(png_sig) && memcmp(header, png_sig, sizeof(png_sig)) == 0;
    }
    if (format == ImageFormat::EPD)
    {
        return headerLength >= 4 && header[0] == 'E' && header[1] == 'P' && header[2] == 'D' && header[3] == '0';
    }
    return false;
}

static void __scan_local_images(std::map<String, LocalImage> &images, bool is_sd, bool prefer_sd)
{
    const char *storageName = is_sd ? "MicroSD" : "LittleFS";
    const uint32_t startMs = millis();
    Log.infoln("[app_download] Scanning local images from %s", storageName);

    HAL::SharedSpiLock sdLock(is_sd);
    if (is_sd && !HAL::GetHAL().sdEnsureReady())
    {
        Log.warningln("[app_download] MicroSD is not ready while scanning images.");
        return;
    }

    File root = is_sd ? HAL::GetHAL().sdOpen("/") : LittleFS.open("/");
    if (!root)
    {
        Log.errorln("[app_download] Failed to open %s root directory while scanning images.", storageName);
        return;
    }
    if (!root.isDirectory())
    {
        Log.errorln("[app_download] %s root path is not a directory while scanning images.", storageName);
        root.close();
        return;
    }

    size_t scannedFiles = 0;
    size_t indexedFiles = 0;
    size_t invalidFiles = 0;
    File file = root.openNextFile();
    while (file)
    {
        scannedFiles++;
        String invalidPath;

        if (!file.isDirectory())
        {
            String fileName = String(file.name());
            ImageFormat format = __image_format_from_name(fileName);
            if (format != ImageFormat::Unknown)
            {
                int slashIndex = fileName.lastIndexOf('/');
                int dotIndex = fileName.lastIndexOf('.');
                if (dotIndex > slashIndex)
                {
                    String fileId = fileName.substring(slashIndex + 1, dotIndex);
                    String fullPath = fileName.startsWith("/") ? fileName : "/" + fileName;
                    if (__local_image_header_valid(file, format))
                    {
                        auto it = images.find(fileId);
                        bool replace = it == images.end() || (is_sd == prefer_sd && it->second.onSd != prefer_sd);
                        if (replace)
                        {
                            images[fileId] = LocalImage{fileId, fullPath, is_sd, format};
                        }
                        indexedFiles++;
                    }
                    else
                    {
                        invalidPath = fullPath;
                        invalidFiles++;
                    }
                }
            }
        }

        file.close();
        if (!invalidPath.isEmpty())
        {
            bool removed = is_sd ? HAL::GetHAL().sdRemove(invalidPath.c_str()) : LittleFS.remove(invalidPath.c_str());
            Log.warningln("[app_download] Removed invalid cached image: %s removed=%d", invalidPath.c_str(), removed ? 1 : 0);
        }
        if ((scannedFiles % 32) == 0)
        {
            delay(0);
        }
        file = root.openNextFile();
    }
    root.close();

    Log.infoln("[app_download] Local image scan completed from %s. scanned=%u, indexed=%u, invalid=%u, elapsed=%lu ms",
               storageName,
               static_cast<unsigned>(scannedFiles),
               static_cast<unsigned>(indexedFiles),
               static_cast<unsigned>(invalidFiles),
               static_cast<unsigned long>(millis() - startMs));
}

static int __download_to_file(const String &url, const String &filePath, bool is_sd)
{
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(HTTP_TIMEOUT_MS);
    HTTPClient http;

    if (!http.begin(client, url))
    {
        Log.errorln("[app_download] HTTP begin failed: %s", url.c_str());
        return static_cast<int>(DownloadResult::HttpBeginFailed);
    }

    String cloud_token = GetCloudToken();
    if (!cloud_token.isEmpty())
    {
        http.addHeader("authorization", cloud_token);
        Log.verboseln("[app_download] Added authorization header to manifest download.");
    }

    http.setTimeout(HTTP_TIMEOUT_MS);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        Log.errorln("[app_download] HTTP GET failed, code %d: %s", httpCode, http.errorToString(httpCode).c_str());
        http.end();
        return httpCode;
    }

    auto remove_partial_file = [&]() {
        bool removed = false;
        if (is_sd)
        {
            HAL::SharedSpiLock lock;
            if (HAL::GetHAL().sdIsReady())
            {
                removed = HAL::GetHAL().sdRemove(filePath.c_str());
            }
        }
        else
        {
            removed = LittleFS.remove(filePath.c_str());
        }
        if (removed)
        {
            Log.verboseln("[app_download] Deleted partial file after manifest download failure: %s", filePath.c_str());
        }
    };

    int written = 0;
    if (is_sd)
    {
        HAL::SharedSpiLock lock;
        if (!HAL::GetHAL().sdEnsureReady())
        {
            Log.errorln("[app_download] MicroSD is not ready for file download: %s", filePath.c_str());
            http.end();
            return static_cast<int>(DownloadResult::FileOpenFailed);
        }
        File f = HAL::GetHAL().sdOpen(filePath.c_str(), FILE_WRITE);
        if (!f)
        {
            Log.errorln("[app_download] Failed to open file on MicroSD: %s", filePath.c_str());
            http.end();
            return static_cast<int>(DownloadResult::FileOpenFailed);
        }
        written = http.writeToStream(&f);
        f.close();
    }
    else
    {
        File f = LittleFS.open(filePath, FILE_WRITE);
        if (!f)
        {
            Log.errorln("[app_download] Failed to open file on LittleFS: %s", filePath.c_str());
            http.end();
            return static_cast<int>(DownloadResult::FileOpenFailed);
        }
        written = http.writeToStream(&f);
        f.close();
    }

    http.end();

    if (written < 0)
    {
        Log.errorln("[app_download] Failed to stream response to %s: %s (%d)",
                    filePath.c_str(), HTTPClient::errorToString(written).c_str(), written);
        remove_partial_file();
        return written;
    }

    if (written == 0)
    {
        Log.errorln("[app_download] Wrote 0 bytes, download considered failed.");
        remove_partial_file();
        return static_cast<int>(DownloadResult::WriteFailed);
    }

    if (is_sd)
    {
        Log.verboseln("[app_download] Successfully wrote %u bytes to MicroSD: %s", (unsigned)written, filePath.c_str());
    }
    else
    {
        Log.verboseln("[app_download] Successfully wrote %u bytes to LittleFS: %s", (unsigned)written, filePath.c_str());
    }
    return HTTP_CODE_OK;
}

static int __fetch_psram_image(const String &url,
                               const char *imageId,
                               int image_index,
                               int image_total,
                               ImageFormat &format)
{
    format = ImageFormat::Unknown;

    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(HTTP_TIMEOUT_MS);
    HTTPClient http;

    if (!http.begin(client, url))
    {
        Log.errorln("[app_download] HTTP begin failed: %s", url.c_str());
        return static_cast<int>(DownloadResult::HttpBeginFailed);
    }
    String cloud_token = GetCloudToken();
    if (!cloud_token.isEmpty())
    {
        http.addHeader("authorization", cloud_token);
        Log.verboseln("[app_download] Added authorization header to image download.");
    }
    http.setTimeout(HTTP_TIMEOUT_MS);

    const char *headerKeys[] = {"x-content-md5", "Content-Type"};
    http.collectHeaders(headerKeys, 2);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        Log.errorln("[app_download] HTTP GET failed, error %d: %s", httpCode, http.errorToString(httpCode).c_str());
        http.end();
        return httpCode;
    }

    if (http.hasHeader("Content-Type"))
    {
        String headerValue = http.header("Content-Type");
        Log.verboseln("[app_download] Content-Type: %s", headerValue.c_str());
    }
    else
    {
        Log.warningln("[app_download] Content-Type header not found.");
    }

    String expected_md5 = http.hasHeader("x-content-md5") ? http.header("x-content-md5") : "";
    if (!expected_md5.isEmpty())
    {
        Log.verboseln("[app_download] Expected MD5 from header: %s", expected_md5.c_str());
    }
    else
    {
        Log.warningln("[app_download] MD5 header 'x-content-md5' not found. Checksum verification will be skipped.");
    }

    size_t imageSize = http.getSize();
    Log.verboseln("[app_download] Image size: %d bytes.", static_cast<int>(imageSize));
    if (imageSize <= 0)
    {
        Log.errorln("[app_download] Content-Length header is missing or zero.");
        http.end();
        return static_cast<int>(DownloadResult::ContentLengthInvalid);
    }
    if (imageSize > 2 * 1024 * 1024)
    {
        Log.errorln("[app_download] Image is too large for PSRAM buffer!");
        http.end();
        return static_cast<int>(DownloadResult::ImageTooLarge);
    }

    if (!ensure_download_buffer(imageSize))
    {
        http.end();
        return static_cast<int>(DownloadResult::PsramAllocFailed);
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t received = 0;
    int last_reported_percent = 0;
    uint32_t last_progress_ms = millis();

    while (http.connected() && received < imageSize)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Log.warningln("[app_download] WiFi disconnected during image download.");
            http.end();
            s_downloadPsramSize = 0;
            return static_cast<int>(DownloadResult::DownloadIncomplete);
        }

        size_t available = stream->available();
        if (available)
        {
            size_t toRead = (received + available > imageSize) ? (imageSize - received) : available;
            int readBytes = stream->read(s_downloadPsramBuffer + received, toRead);
            if (readBytes > 0)
            {
                received += readBytes;
                last_progress_ms = millis();
                int current_percent = (received * 100) / imageSize;

                if (current_percent >= last_reported_percent + 20)
                {
                    last_reported_percent = (current_percent / 20) * 20;

                    Log.infoln("[app_download] Download progress: %d%% (%d / %d bytes)", last_reported_percent, received, imageSize);

                    s_imageRefreshData.apply = false;
                    s_imageRefreshData.download_progress = last_reported_percent;
                    strncpy(s_imageRefreshData.version_id, imageId, IMAGE_ID_MAX_LEN - 1);
                    s_imageRefreshData.version_id[IMAGE_ID_MAX_LEN - 1] = '\0';
                    s_imageRefreshData.image_index = image_index;
                    s_imageRefreshData.image_total = image_total;
                    imgRefreshRes(s_batchVersion.c_str(), false, last_reported_percent, s_imageRefreshData.version_id, s_imageRefreshData.image_index, s_imageRefreshData.image_total);
                }
            }
        }
        else if (millis() - last_progress_ms > DOWNLOAD_IDLE_TIMEOUT_MS)
        {
            Log.errorln("[app_download] Image download stalled for %u ms at %u/%u bytes.",
                        static_cast<unsigned>(DOWNLOAD_IDLE_TIMEOUT_MS),
                        static_cast<unsigned>(received),
                        static_cast<unsigned>(imageSize));
            http.end();
            s_downloadPsramSize = 0;
            return static_cast<int>(DownloadResult::DownloadIncomplete);
        }
        delay(5);
    }
    http.end();

    if (received != imageSize)
    {
        Log.errorln("[app_download] Download incomplete. Expected %d, got %d.", imageSize, received);
        s_downloadPsramSize = 0;
        return static_cast<int>(DownloadResult::DownloadIncomplete);
    }

    Log.verboseln("[app_download] Download progress: 100%%. Successfully downloaded %u bytes to PSRAM.", received);
    s_downloadPsramSize = received;
    format = __image_format_from_buffer(s_downloadPsramBuffer, s_downloadPsramSize);
    if (format == ImageFormat::Unknown)
    {
        Log.errorln("[app_download] Unsupported image file header.");
        s_downloadPsramSize = 0;
        return static_cast<int>(DownloadResult::ImageFormatUnknown);
    }

    if (s_downloadPsramSize > 0 && !expected_md5.isEmpty())
    {
        unsigned char md5_output[16];
        mbedtls_md5(s_downloadPsramBuffer, s_downloadPsramSize, md5_output);

        char calculated_md5_str[33];
        for (int i = 0; i < 16; i++)
        {
            sprintf(&calculated_md5_str[i * 2], "%02x", (int)md5_output[i]);
        }
        calculated_md5_str[32] = '\0';
        Log.verboseln("[app_download] Calculated MD5 of downloaded data: %s", calculated_md5_str);

        if (!expected_md5.equalsIgnoreCase(calculated_md5_str))
        {
            Log.errorln("[app_download] MD5 CHECKSUM MISMATCH! Expected: %s, Calculated: %s", expected_md5.c_str(), calculated_md5_str);
            s_downloadPsramSize = 0;
            return static_cast<int>(DownloadResult::Md5Mismatch);
        }
        Log.verboseln("[app_download] MD5 checksum match!");
    }
    return HTTP_CODE_OK;
}

static bool __save_psram_image(const String &filePath, bool isToSd)
{
    if (!s_downloadPsramBuffer || s_downloadPsramSize == 0)
    {
        Log.errorln("[app_download] PSRAM buffer is empty or invalid, cannot save.");
        return false;
    }

    File f;
    const char *storageName = isToSd ? "Micro SD" : "LittleFS";
    Log.verboseln("[app_download] Preparing to write to %s: %s", storageName, filePath.c_str());

    HAL::SharedSpiLock sdLock(isToSd);
    if (isToSd)
    {
        if (!HAL::GetHAL().sdEnsureReady())
        {
            Log.errorln("[app_download] MicroSD is not ready for image save: %s", filePath.c_str());
            return false;
        }
        f = HAL::GetHAL().sdOpen(filePath.c_str(), FILE_WRITE);
    }
    else
    {
        f = LittleFS.open(filePath, FILE_WRITE);
    }

    if (!f)
    {
        Log.errorln("[app_download] Failed to open file! Storage: %s, Path: %s", storageName, filePath.c_str());
        if (isToSd)
        {
            HAL::GetHAL().sdDeinit();
        }
        return false;
    }

    size_t remaining = s_downloadPsramSize;
    size_t offset = 0;

    while (remaining > 0)
    {
        size_t toWrite = (remaining < DOWNLOAD_WRITE_CHUNK_SIZE) ? remaining : DOWNLOAD_WRITE_CHUNK_SIZE;
        size_t written_now = 0;
        bool chunk_written_successfully = false;

        for (int retry = 0; retry < FS_WRITE_MAX_RETRIES; ++retry)
        {
            errno = 0;
            written_now = f.write(s_downloadPsramBuffer + offset, toWrite);
            if (written_now == toWrite)
            {
                chunk_written_successfully = true;
                break;
            }
            Log.warningln("[app_download] Chunk write failed on %s. Expected %u, wrote %u, errno=%d. Retrying %d/%d...",
                          storageName,
                          static_cast<unsigned>(toWrite),
                          static_cast<unsigned>(written_now),
                          errno,
                          retry + 1,
                          FS_WRITE_MAX_RETRIES);
            delay(FS_WRITE_RETRY_DELAY_MS);
        }

        if (!chunk_written_successfully)
        {
            Log.errorln("[app_download] Chunk write failed after multiple retries. Aborting save.");
            f.close();
            if (isToSd)
            {
                HAL::GetHAL().sdRemove(filePath.c_str());
                HAL::GetHAL().sdDeinit();
            }
            else
            {
                LittleFS.remove(filePath.c_str());
            }
            Log.verboseln("[app_download] Deleted partially written file: %s", filePath.c_str());
            return false;
        }

        offset += written_now;
        remaining -= written_now;
        delay(5);
    }
    f.close();

    if (!IsTimerWakeup())
        hal_indicator_play(HAL_INDICATOR_IMAGE_DOWNLOAD);

    Log.infoln("[app_download] Successfully saved %u bytes to %s on %s.",
               static_cast<unsigned>(offset),
               filePath.c_str(),
               storageName);
    SetHasImage(true);
    return true;
}

static void __prune_images(const std::set<String> &validImageIds, bool is_sd)
{
    const char *storageName = is_sd ? "MicroSD" : "LittleFS";
    const uint32_t startMs = millis();
    Log.infoln("[app_download] Starting cleanup of obsolete images on %s", storageName);

    std::vector<String> filesToDelete;
    HAL::SharedSpiLock sdLock(is_sd);
    if (is_sd && !HAL::GetHAL().sdEnsureReady())
    {
        Log.warningln("[app_download] MicroSD is not ready for cleanup.");
        return;
    }

    File root = is_sd ? HAL::GetHAL().sdOpen("/") : LittleFS.open("/");
    if (!root)
    {
        Log.errorln("[app_download] Failed to open %s root directory for cleanup.", storageName);
        return;
    }
    if (!root.isDirectory())
    {
        Log.errorln("[app_download] %s root path is not a directory.", storageName);
        root.close();
        return;
    }

    size_t scannedFiles = 0;
    File file = root.openNextFile();
    while (file)
    {
        scannedFiles++;
        if (!file.isDirectory())
        {
            String fileName = String(file.name());
            if (__is_image_file(fileName))
            {
                int slashIndex = fileName.lastIndexOf('/');
                int dotIndex = fileName.lastIndexOf('.');
                if (dotIndex > slashIndex)
                {
                    String fileId = fileName.substring(slashIndex + 1, dotIndex);

                    if (validImageIds.find(fileId) == validImageIds.end())
                    {
                        String fullPath = fileName.startsWith("/") ? fileName : "/" + fileName;
                        filesToDelete.push_back(fullPath);
                    }
                }
            }
        }
        file.close();
        if ((scannedFiles % 32) == 0)
        {
            delay(0);
        }
        file = root.openNextFile();
    }
    root.close();

    Log.infoln("[app_download] Cleanup scan on %s finished. scanned=%u, obsolete=%u",
               storageName,
               static_cast<unsigned>(scannedFiles),
               static_cast<unsigned>(filesToDelete.size()));

    if (!filesToDelete.empty())
    {
        Log.verboseln("[app_download] Found %d obsolete images to delete.", filesToDelete.size());
        size_t deletedCount = 0;
        for (const String &path : filesToDelete)
        {
            Log.verboseln("[app_download] Deleting: %s", path.c_str());
            bool ok = is_sd ? HAL::GetHAL().sdRemove(path.c_str()) : LittleFS.remove(path.c_str());
            if (!ok)
            {
                Log.errorln("[app_download] Failed to delete file: %s", path.c_str());
            }
            else
            {
                _new_task = true;
                deletedCount++;
            }
            delay(0);
        }
        Log.infoln("[app_download] Cleanup delete phase on %s finished. deleted=%u", storageName, static_cast<unsigned>(deletedCount));
    }
    else
    {
        Log.verboseln("[app_download] No obsolete images found to delete.");
    }

    Log.infoln("[app_download] Cleanup completed on %s in %lu ms", storageName, static_cast<unsigned long>(millis() - startMs));
}

static int __parse_manifest_tasks()
{
    const char *manifestPath = "/manifest.json";
    const uint32_t parseStartMs = millis();
    File manifestFile = LittleFS.open(manifestPath, FILE_READ);

    if (!manifestFile)
    {
        Log.errorln("[app_download] Failed to read manifest.json from LittleFS.");
        return static_cast<int>(DownloadResult::ManifestReadFailed);
    }

    size_t fileSize = manifestFile.size();
    if (fileSize == 0)
    {
        Log.errorln("[app_download] Manifest file is empty on LittleFS.");
        manifestFile.close();
        return static_cast<int>(DownloadResult::ManifestParseFailed);
    }
    const size_t manifestMaxSize = MANIFEST_MAX_SIZE_BYTES;
    if (manifestMaxSize > 0 && fileSize > manifestMaxSize)
    {
        Log.errorln("[app_download] Manifest too large (%u bytes). Max allowed is %u bytes.",
                    static_cast<unsigned>(fileSize), static_cast<unsigned>(manifestMaxSize));
        manifestFile.close();
        return static_cast<int>(DownloadResult::ManifestParseFailed);
    }
    Log.infoln("[app_download] Manifest file size on %s: %u bytes",
               "LittleFS",
               static_cast<unsigned>(fileSize));

    if (g_psramManifestBuffer)
        free(g_psramManifestBuffer);
    g_psramManifestBuffer = (char *)ps_malloc(fileSize + 1);
    if (!g_psramManifestBuffer)
    {
        Log.errorln("[app_download] PSRAM allocation failed for manifest buffer.");
        manifestFile.close();
        return static_cast<int>(DownloadResult::PsramAllocFailed);
    }

    manifestFile.readBytes(g_psramManifestBuffer, fileSize);
    g_psramManifestBuffer[fileSize] = '\0';
    g_psramManifestSize = fileSize;
    manifestFile.close();
    Log.infoln("[app_download] Manifest file loaded into memory: %u bytes", static_cast<unsigned>(g_psramManifestSize));

    cJSON *root = cJSON_Parse(g_psramManifestBuffer);
    if (root == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            Log.errorln("[app_download] Manifest parsing failed near: %s", error_ptr);
        }
        else
        {
            Log.errorln("[app_download] Manifest parsing failed. Invalid JSON format.");
        }
        return static_cast<int>(DownloadResult::ManifestParseFailed);
    }
    Log.infoln("[app_download] Manifest JSON parsed successfully");

    const cJSON *album_id_json = cJSON_GetObjectItemCaseSensitive(root, "album_id");
    if (cJSON_IsString(album_id_json) && (album_id_json->valuestring != NULL))
    {
        Log.verboseln("[app_download] Manifest parsed successfully: album_id=%s", album_id_json->valuestring);
    }

    const cJSON *images = cJSON_GetObjectItemCaseSensitive(root, "images");
    if (!cJSON_IsArray(images))
    {
        Log.errorln("[app_download] 'images' array not found in manifest.");
        cJSON_Delete(root);
        return static_cast<int>(DownloadResult::ManifestImagesMissing);
    }

    int manifest_count = cJSON_GetArraySize(images);
    Log.infoln("[app_download] Manifest contains %d image entries", manifest_count);
    s_imageTasks.clear();
    s_totalImagesInManifest = 0;
    _new_task = false;

    std::vector<ManifestImage> manifestImages;
    manifestImages.reserve(manifest_count);
    std::set<String> validImageIds;

    if (manifest_count == 0)
    {
        cJSON_Delete(root);
        return static_cast<int>(DownloadResult::ManifestEmpty);
    }

    bool all_static = true;

    int manifest_order = 0;
    cJSON *image = NULL;
    cJSON_ArrayForEach(image, images)
    {
        const cJSON *imageUrl_json = cJSON_GetObjectItemCaseSensitive(image, "url");
        const cJSON *imgId_json = cJSON_GetObjectItemCaseSensitive(image, "id");
        if (!cJSON_IsString(imageUrl_json) || imageUrl_json->valuestring == nullptr ||
            !cJSON_IsString(imgId_json) || imgId_json->valuestring == nullptr)
        {
            Log.warningln("[app_download] Invalid image entry in manifest at order %d. Skipping.", manifest_order);
            manifest_order++;
            continue;
        }

        String image_id = imgId_json->valuestring;
        if (validImageIds.find(image_id) != validImageIds.end())
        {
            Log.warningln("[app_download] Duplicate image id ignored: %s", image_id.c_str());
            manifest_order++;
            continue;
        }

        String url = imageUrl_json->valuestring;
        ResourceKind kind = __resource_kind_from_url(url);
        all_static = all_static && kind == ResourceKind::StaticImage;

        ManifestImage manifestImage;
        manifestImage.id = image_id;
        manifestImage.url = url;
        manifestImage.order = manifestImages.size();
        manifestImage.kind = kind;
        manifestImages.push_back(manifestImage);

        validImageIds.insert(manifestImage.id);

        manifest_order++;

        if ((manifest_order % 16) == 0)
        {
            delay(0);
        }
    }

    if (manifestImages.empty())
    {
        cJSON_Delete(root);
        return static_cast<int>(DownloadResult::ManifestEntriesInvalid);
    }

    const bool gallery_content = all_static;

    Log.infoln("[app_download] Manifest metadata scan completed. valid=%u, gallery=%d, dashboard=%d, all_static=%d",
               static_cast<unsigned>(manifestImages.size()),
               gallery_content ? 1 : 0,
               gallery_content ? 0 : 1,
               all_static ? 1 : 0);

    s_totalImagesInManifest = manifestImages.size();

    bool sd_inserted = HAL::GetHAL().sdIsInserted();
    bool sd_ready = sd_inserted && HAL::GetHAL().sdEnsureReady();
    use_sdcard = all_static && sd_ready;
    Log.infoln("[app_download] Storage decision: all_static=%d, sd_inserted=%d, sd_mounted=%d, sd_ready=%d, use_sdcard=%d",
               all_static ? 1 : 0,
               sd_inserted ? 1 : 0,
               HAL::GetHAL().sdIsMounted() ? 1 : 0,
               sd_ready ? 1 : 0,
               use_sdcard ? 1 : 0);
    if (sd_inserted && !sd_ready)
    {
        Log.warningln("[app_download] MicroSD is inserted but not ready, images will be stored on LittleFS.");
    }

    bool device_mode_was_gallery = IsGalleryContent();
    if (gallery_content)
    {
        Log.verboseln("[app_download] >>> Gallery Mode <<<");
        SetContentMode(AppContentMode::Gallery);
        SetLocalGallerySession(true);
    }
    else
    {
        Log.verboseln("[app_download] >>> Dashboard Mode <<<");
        SetContentMode(AppContentMode::Dashboard);
        SetLocalGallerySession(false);
    }

    SetImageCount(s_totalImagesInManifest);

    if (!gallery_content && device_mode_was_gallery)
    {
        SetCurrentImageId("");
        SetCurrentImageOrder(0);
    }

    std::map<String, LocalImage> localImages;
    __scan_local_images(localImages, false, use_sdcard);
    if (sd_ready)
    {
        __scan_local_images(localImages, true, use_sdcard);
    }

    __prune_images(validImageIds, false);
    if (sd_ready)
    {
        __prune_images(validImageIds, true);
    }

    String only_id = app_download_only_id();
    if (!only_id.isEmpty())
    {
        Log.infoln("[app_download] Target image refresh requested: id=%s", only_id.c_str());
    }

    s_imageTasks.reserve(only_id.isEmpty() ? manifestImages.size() : 1);
    for (const ManifestImage &manifestImage : manifestImages)
    {
        if (!only_id.isEmpty() && !manifestImage.id.equals(only_id))
        {
            continue;
        }

        ImageTask task;
        task.id = manifestImage.id;
        task.url = manifestImage.url;
        task.order = manifestImage.order;
        task.storeOnSd = use_sdcard;
        task.format = ImageFormat::Unknown;
        task.kind = manifestImage.kind;

        auto local = localImages.find(manifestImage.id);
        if (__can_reuse_manifest_image(manifestImage) && local != localImages.end())
        {
            task.type = ImageTaskType::Reuse;
            task.path = local->second.path;
            task.storeOnSd = local->second.onSd;
            task.format = local->second.format;
            Log.verboseln("[app_download] Reusing image id=%s path=%s",
                          task.id.c_str(),
                          task.path.c_str());
        }
        else
        {
            task.type = ImageTaskType::Download;
            _new_task = true;
            Log.verboseln("[app_download] Queued image id=%s for download (%s, kind=%d)",
                          task.id.c_str(),
                          task.storeOnSd ? "SD" : "LittleFS",
                          static_cast<int>(manifestImage.kind));
        }

        s_imageTasks.push_back(task);
        if ((s_imageTasks.size() % 16) == 0)
        {
            delay(0);
        }
    }

    if (!only_id.isEmpty() && s_imageTasks.empty())
    {
        Log.warningln("[app_download] Target image id not found in manifest: %s", only_id.c_str());
    }

    Log.infoln("[app_download] Image task queue prepared. queued=%u", static_cast<unsigned>(s_imageTasks.size()));

    app_gallery_rebuild_cache_from_manifest();
    Log.infoln("[app_download] Manifest task preparation completed in %lu ms",
               static_cast<unsigned long>(millis() - parseStartMs));

    cJSON_Delete(root);
    return HTTP_CODE_OK;
}

namespace
{
    class DownloadApp final
    {
    public:
        const char *name() const { return "DownloadApp"; }

        void start()
        {
            if (!_started)
            {
                Log.verboseln("[app_download] Download app started");
                _started = true;
            }
        }

        void run(uint32_t now_ms)
        {
            if (!__download_event_group)
            {
                return;
            }

            process_event_bits();
            process_manifest_download(now_ms);
            process_manifest_parse(now_ms);
            process_image_download(now_ms);
        }

    private:
        void reset_all_tasks()
        {
            Log.verboseln("[app_download] Cancel request received, resetting all download tasks...");

            // Clear the image download task queue
            s_imageTasks.clear();
            s_imageTasks.shrink_to_fit();

            // Reset all state flags and variables
            _pending_manifest_download = false;
            _pending_manifest_parse = false;
            _downloading_images = false;
            _image_in_progress = false;
            _waiting_wifi_manifest = false;
            _waiting_wifi_image = false;
            _batch_failed = false;
            _empty_manifest_retries = 0;
            _waiting_page_shown = false;
            s_batchVersion = "";

            _task_index = 0;
            _current_retry = 0;
            _manifest_wait_until = 0;
            _image_wait_until = 0;
            s_currentTaskIndex = 0;

            app_download_set_active(false);
            app_download_release_power();
            Log.verboseln("[app_download] All tasks have been cancelled and state has been reset.");
        }

        void process_event_bits()
        {
            EventBits_t bits = xEventGroupWaitBits(
                __download_event_group,
                DL_EVT_JSON_DOWNLOAD | DL_EVT_JSON_PARSE | DL_EVT_IMAGE_DOWNLOAD | DL_EVT_TASK_FINISHED | DL_EVT_TASK_CANCEL,
                pdTRUE,
                pdFALSE,
                0);

            if (bits == 0)
            {
                return;
            }

            if (bits & DL_EVT_TASK_CANCEL)
            {
                reset_all_tasks();
            }

            if (bits & DL_EVT_JSON_DOWNLOAD)
            {
                app_download_set_active(true);
                app_download_acquire_power();
                String manifest_url = app_download_manifest_url();
#if (RETERMINAL_INFO_DEBUG)
                Log.infoln("[app_download] Manifest URL: %s", manifest_url.c_str());
#endif
                Log.verboseln("[app_download] Manifest URL: %s", manifest_url.c_str());
                xSemaphoreTake(s_download_state_mutex, portMAX_DELAY);
                s_batchVersion = s_manifestVersion;
                xSemaphoreGive(s_download_state_mutex);
                _pending_manifest_download = true;
                _manifest_wait_until = 0;
                _waiting_wifi_manifest = false;
                _empty_manifest_retries = 0;
                _waiting_page_shown = false;
            }

            if (bits & DL_EVT_JSON_PARSE)
            {
                _pending_manifest_parse = true;
            }

            if (bits & DL_EVT_IMAGE_DOWNLOAD)
            {
                Log.verboseln("[app_download] Event: Starting image downloads. Total tasks: %d", s_imageTasks.size());
                _downloading_images = true;
                _task_index = s_currentTaskIndex;
                _current_retry = 0;
                _image_in_progress = false;
                _waiting_wifi_image = false;
                _image_wait_until = 0;
                _batch_failed = false;
                StopDeepSleepTimer();
            }

            if (bits & DL_EVT_TASK_FINISHED)
            {
                handle_task_finished();
            }
        }

        void process_manifest_download(uint32_t now_ms)
        {
            if (!_pending_manifest_download)
            {
                return;
            }

            if (now_ms < _manifest_wait_until)
            {
                return;
            }

            if (WiFi.status() != WL_CONNECTED)
            {
                if (!_waiting_wifi_manifest)
                {
                    Log.verboseln("[app_download] WiFi disconnected, waiting before downloading manifest...");
                    _waiting_wifi_manifest = true;
                }
                _manifest_wait_until = now_ms + 1000;
                return;
            }

            if (_waiting_wifi_manifest)
            {
                Log.verboseln("[app_download] WiFi reconnected, continuing manifest download.");
                _waiting_wifi_manifest = false;
            }

            String manifest_url = app_download_manifest_url();
            Log.infoln("[app_download] Starting manifest download from: %s", manifest_url.c_str());
            int result = __download_to_file(manifest_url, "/manifest.json", false);
            if (result == HTTP_CODE_OK)
            {
                Log.infoln("[app_download] Manifest downloaded successfully: /manifest.json");
                esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_JSON_PARSE, NULL, 0, portMAX_DELAY);
            }
            else
            {
                Log.errorln("[app_download] Manifest download failed with code %d. Terminating batch.", result);
                esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_FAILED, NULL, 0, portMAX_DELAY);
            }

            _pending_manifest_download = false;
        }

        void process_manifest_parse(uint32_t now_ms)
        {
            if (!_pending_manifest_parse)
            {
                return;
            }

            Log.infoln("[app_download] Starting manifest parse: /manifest.json");
            int result = __parse_manifest_tasks();
            if (result == HTTP_CODE_OK)
            {
                Log.infoln("[app_download] Manifest parsed successfully. Queued %d image task(s).", s_imageTasks.size());
                _empty_manifest_retries = 0;
                s_currentTaskIndex = 0;
                esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_IMAGE_DOWNLOAD, NULL, 0, portMAX_DELAY);
            }
            else if (result == static_cast<int>(DownloadResult::ManifestEmpty))
            {
                if (!_waiting_page_shown)
                {
                    esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_WAITING, NULL, 0, portMAX_DELAY);
                    _waiting_page_shown = true;
                }

                if (_empty_manifest_retries < MAX_EMPTY_MANIFEST_RETRIES)
                {
                    _empty_manifest_retries++;
                    uint32_t retry_delay_ms = 10000;
                    if (_empty_manifest_retries == 1)
                    {
                        retry_delay_ms = 2000;
                    }
                    else if (_empty_manifest_retries == 2)
                    {
                        retry_delay_ms = 5000;
                    }
                    Log.warningln("[app_download] Manifest is empty, retry %d/%d in %u ms.",
                                  _empty_manifest_retries,
                                  MAX_EMPTY_MANIFEST_RETRIES,
                                  retry_delay_ms);
                    _pending_manifest_download = true;
                    _manifest_wait_until = now_ms + retry_delay_ms;
                }
                else
                {
                    Log.warningln("[app_download] Manifest remains empty after retries.");
                    app_download_set_active(false);
                    app_download_release_power();
                    s_batchVersion = "";
                }
            }
            else
            {
                Log.errorln("[app_download] Manifest parsing failed with code %d. Terminating batch.", result);
                esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_FAILED, NULL, 0, portMAX_DELAY);
            }

            _pending_manifest_parse = false;
        }

        void process_image_download(uint32_t now_ms)
        {
            if (!_downloading_images)
            {
                return;
            }

            if (_task_index >= s_imageTasks.size())
            {
                Log.infoln("[app_download] All images from the manifest have been processed successfully.");
                _new_task = false;

                esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_FINISHED, NULL, 0, portMAX_DELAY);
                _downloading_images = false;
                return;
            }

            if (now_ms < _image_wait_until)
            {
                return;
            }

            if (WiFi.status() != WL_CONNECTED)
            {
                if (!_waiting_wifi_image)
                {
                    Log.verboseln("[app_download] WiFi disconnected, pausing download and waiting for reconnection...");
                    _waiting_wifi_image = true;
                }
                _image_wait_until = now_ms + 1000;
                return;
            }

            if (_waiting_wifi_image)
            {
                Log.verboseln("[app_download] WiFi reconnected, continuing download...");
                _waiting_wifi_image = false;
            }

            if (!_image_in_progress)
            {
                _image_in_progress = true;
            }

            const ImageTask &task = s_imageTasks[_task_index];

            if (task.type == ImageTaskType::Reuse)
            {
                Log.infoln("[app_download] --> Existing image already present (%d/%d of total %d)",
                           task.order + 1, s_totalImagesInManifest, s_totalImagesInManifest);

                SetHasImage(true);
                app_gallery_mark_available(task.id.c_str(), task.path.c_str(), task.storeOnSd, task.kind != ResourceKind::StaticImage);
                s_imageRefreshData.apply = true;
                s_imageRefreshData.download_progress = 100;
                strncpy(s_imageRefreshData.version_id, task.id.c_str(), IMAGE_ID_MAX_LEN - 1);
                s_imageRefreshData.version_id[IMAGE_ID_MAX_LEN - 1] = '\0';
                s_imageRefreshData.image_index = task.order + 1;
                s_imageRefreshData.image_total = s_totalImagesInManifest;
                imgRefreshRes(s_batchVersion.c_str(),
                              true,
                              s_imageRefreshData.download_progress,
                              s_imageRefreshData.version_id,
                              s_imageRefreshData.image_index,
                              s_imageRefreshData.image_total);

                _task_index++;
                s_currentTaskIndex = _task_index;
                _current_retry = 0;
                _image_wait_until = now_ms + 10;
                _image_in_progress = false;
                return;
            }

            Log.infoln("[app_download] --> Starting Image download (%d/%d of total %d)", task.order + 1, s_totalImagesInManifest, s_totalImagesInManifest);
#if (RETERMINAL_INFO_DEBUG)
            Log.verboseln("[app_download] Download URL: %s", task.url.c_str());
#endif
            Log.verboseln("[app_download] Download URL: %s", task.url.c_str());

            bool is_success = false;
            int result_code = 0;

            ImageFormat imageFormat = ImageFormat::Unknown;
            result_code = __fetch_psram_image(task.url, task.id.c_str(), task.order + 1, s_totalImagesInManifest, imageFormat);
            if (result_code == HTTP_CODE_OK)
            {
                const String finalPath = __image_path(task.id, imageFormat);
                bool save_to_sd = task.storeOnSd && HAL::GetHAL().sdEnsureReady();
                if (task.storeOnSd && !save_to_sd)
                {
                    Log.warningln("[app_download] MicroSD became unavailable before saving image, falling back to LittleFS.");
                }

                bool saved_ok = false;
                if (imgBufferMutex_lock())
                {
                    saved_ok = __save_psram_image(finalPath, save_to_sd);
                    if (!saved_ok && save_to_sd)
                    {
                        Log.warningln("[app_download] Failed to save image on MicroSD, retrying on LittleFS.");
                        saved_ok = __save_psram_image(finalPath, false);
                    }
                    imgBufferMutex_unlock();
                }
                else
                {
                    Log.errorln("[app_download] Failed to lock image buffer mutex before saving image.");
                }

                if (saved_ok)
                {
                    is_success = true;
                    __remove_other_image_variants(task.id, finalPath, save_to_sd);
                    app_gallery_mark_available(task.id.c_str(), finalPath.c_str(), save_to_sd, task.kind != ResourceKind::StaticImage);
                    s_imageRefreshData.apply = true;
                    s_imageRefreshData.download_progress = 100;
                    strncpy(s_imageRefreshData.version_id, task.id.c_str(), IMAGE_ID_MAX_LEN - 1);
                    s_imageRefreshData.version_id[IMAGE_ID_MAX_LEN - 1] = '\0';
                    s_imageRefreshData.image_index = task.order + 1;
                    s_imageRefreshData.image_total = s_totalImagesInManifest;
                    imgRefreshRes(s_batchVersion.c_str(), true, s_imageRefreshData.download_progress, s_imageRefreshData.version_id, s_imageRefreshData.image_index, s_imageRefreshData.image_total);
                }
                else
                {
                    Log.errorln("[app_download] Failed to save image from PSRAM to filesystem!");
                    result_code = static_cast<int>(DownloadResult::WriteFailed);
                }
            }

            if (is_success)
            {
                Log.verboseln("[app_download] Task completed successfully.");
                Log.verboseln("[app_download] Downloaded image from URL: %s", task.url.c_str());
                _task_index++;
                s_currentTaskIndex = _task_index;
                _current_retry = 0;
                _image_wait_until = now_ms + 10;
                _image_in_progress = false;
            }
            else
            {
                _current_retry++;
                Log.errorln("[app_download] Task failed (code %d), retry %d/%d for: %s", result_code, _current_retry, MAX_DOWNLOAD_RETRIES, task.url.c_str());
                if (_current_retry >= MAX_DOWNLOAD_RETRIES)
                {
                    Log.warningln("[app_download] Task reached max retries and failed. Aborting batch.");
                    Log.verboseln("[app_download] Failed task URL: %s", task.url.c_str());
                    _downloading_images = false;
                    _image_in_progress = false;
                    _batch_failed = true;
                    esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_FINISHED, NULL, 0, portMAX_DELAY);
                }
                else
                {
                    _image_wait_until = now_ms + 1000;
                }
            }
        }

        void handle_task_finished()
        {
            Log.verboseln("[app_download] Event: Download task batch finished. Awaiting new instructions.");

            bool has_image = HasImage();

            app_download_set_active(false);
            app_download_release_power();

            if (has_image)
            {
                if (!_batch_failed)
                {
                    SetContentVersion(s_batchVersion);
                    SetAlbumVersion(s_batchVersion);
                }

                esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, NULL, 1, portMAX_DELAY);
            }

            s_imageTasks.clear();
            s_imageTasks.shrink_to_fit();

            _downloading_images = false;
            _pending_manifest_download = false;
            _pending_manifest_parse = false;
            _waiting_wifi_manifest = false;
            _waiting_wifi_image = false;
            _image_in_progress = false;
            _batch_failed = false;
            _empty_manifest_retries = 0;
            _waiting_page_shown = false;
            _current_retry = 0;
            _task_index = 0;
            _manifest_wait_until = 0;
            _image_wait_until = 0;
            s_batchVersion = "";
        }

        bool _started{false};
        bool _pending_manifest_download{false};
        bool _pending_manifest_parse{false};
        bool _downloading_images{false};
        bool _waiting_wifi_manifest{false};
        bool _waiting_wifi_image{false};
        bool _image_in_progress{false};
        bool _batch_failed{false};
        bool _waiting_page_shown{false};
        uint32_t _manifest_wait_until{0};
        uint32_t _image_wait_until{0};
        size_t _task_index{0};
        int _current_retry{0};
        int _empty_manifest_retries{0};
    };

    DownloadApp g_download_app;
    bool g_download_task_registered = false;
    TaskHandle_t g_download_task_handle = nullptr;

    void download_task(void *)
    {
        while (true)
        {
            g_download_app.run(millis());
            vTaskDelay(DOWNLOAD_TASK_INTERVAL);
        }
    }
}

static void __handle_event(void *handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == DOWNLOAD_EVENT_BASE)
    {
        switch (event_id)
        {
        case DOWNLOAD_EVT_JSON_DOWNLOAD:
            Log.verboseln("DOWNLOAD_EVT_JSON_DOWNLOAD");
            xEventGroupSetBits(__download_event_group, DL_EVT_JSON_DOWNLOAD);

            break;

        case DOWNLOAD_EVT_JSON_PARSE:
            Log.verboseln("DOWNLOAD_EVT_JSON_PARSE");
            xEventGroupSetBits(__download_event_group, DL_EVT_JSON_PARSE);
            break;

        case DOWNLOAD_EVT_IMAGE_DOWNLOAD:
            Log.verboseln("DOWNLOAD_EVT_IMAGE_DOWNLOAD");
            xEventGroupSetBits(__download_event_group, DL_EVT_IMAGE_DOWNLOAD);
            break;

        case DOWNLOAD_EVT_TASK_FINISHED:
            Log.verboseln("DOWNLOAD_EVT_TASK_FINISHED");

            xEventGroupSetBits(__download_event_group, DL_EVT_TASK_FINISHED);

            break;

        case DOWNLOAD_EVT_TASK_PAUSE:
            Log.verboseln("DOWNLOAD_EVT_TASK_PAUSE");
            break;

        case DOWNLOAD_EVT_TASK_FAILED:
            Log.warningln("DOWNLOAD_EVT_TASK_FAILED");

            __fallback_to_cached_image_after_failure();

            break;

        case DOWNLOAD_EVT_TASK_CANCEL:
            Log.verboseln("DOWNLOAD_EVT_TASK_CANCEL");
            xEventGroupSetBits(__download_event_group, DL_EVT_TASK_CANCEL);
            break;

        default:
            break;
        }
    }
}

void app_download_set_manifest_url(const String &url, const String &version, const char *only_id)
{
    app_power_manager_activity(AppPowerOwner::Download);
    esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_CANCEL, NULL, 0, portMAX_DELAY);
    xSemaphoreTake(s_download_state_mutex, portMAX_DELAY);
    s_manifestUrl = url;
    s_manifestVersion = version;
    s_onlyId = only_id ? only_id : "";
    s_download_active = true;
    xSemaphoreGive(s_download_state_mutex);
    esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_JSON_DOWNLOAD, NULL, 1, portMAX_DELAY);
}

bool app_download_refresh_image(const char *id)
{
    if (!id || id[0] == '\0')
    {
        return false;
    }

    xSemaphoreTake(s_download_state_mutex, portMAX_DELAY);
    bool ready = !s_manifestUrl.isEmpty();
    if (ready)
    {
        s_onlyId = id;
        s_download_active = true;
    }
    xSemaphoreGive(s_download_state_mutex);

    if (!ready)
    {
        Log.warningln("[app_download] Cannot refresh image without manifest URL: id=%s", id);
        return false;
    }

    app_power_manager_activity(AppPowerOwner::Download);
    esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_CANCEL, NULL, 0, portMAX_DELAY);
    esp_event_post(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_JSON_DOWNLOAD, NULL, 1, portMAX_DELAY);
    return true;
}

bool app_download_is_same_manifest_in_progress(const char *version, const char *url)
{
    if (!version || !url)
    {
        return false;
    }

    xSemaphoreTake(s_download_state_mutex, portMAX_DELAY);
    bool active = s_download_active;
    bool same_manifest = active && s_manifestVersion.equals(version) && s_manifestUrl.equals(url);
    xSemaphoreGive(s_download_state_mutex);

    return same_manifest;
}

void app_download_init()
{
    if (g_download_task_registered)
    {
        Log.warningln("[app_download] Download task already running.");
        return;
    }

    __download_event_group = xEventGroupCreate();
    if (!__download_event_group)
    {
        Log.fatalln("[app_download] Failed toeate event group!");
        return;
    }
    esp_event_handler_register(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_JSON_DOWNLOAD, &__handle_event, NULL);
    esp_event_handler_register(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_JSON_PARSE, &__handle_event, NULL);
    esp_event_handler_register(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_IMAGE_DOWNLOAD, &__handle_event, NULL);
    esp_event_handler_register(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_FINISHED, &__handle_event, NULL);
    esp_event_handler_register(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_PAUSE, &__handle_event, NULL);
    esp_event_handler_register(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_FAILED, &__handle_event, NULL);
    esp_event_handler_register(DOWNLOAD_EVENT_BASE, DOWNLOAD_EVT_TASK_CANCEL, &__handle_event, NULL);

    g_download_app.start();

    BaseType_t task_created = xTaskCreate(
        download_task,
        "app_download",
        DOWNLOAD_TASK_STACK_SIZE,
        nullptr,
        DOWNLOAD_TASK_PRIORITY,
        &g_download_task_handle);
    if (task_created != pdPASS)
    {
        g_download_task_handle = nullptr;
        Log.errorln("[app_download] Failed to create download task.");
        return;
    }

    g_download_task_registered = true;
}
