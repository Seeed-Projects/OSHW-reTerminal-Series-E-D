#include "APP/app_gallery.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

#include "ArduinoLog.h"
#include "cJSON.h"

#include "APP/app_device_info.h"
#include "app_config.h"
#include "hal/hal.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum class GalleryMove
{
    Prev,
    Next,
};

static std::vector<app_gallery_image_ref_t> s_images;
static SemaphoreHandle_t s_mutex = nullptr;

static bool ensure_mutex()
{
    if (s_mutex)
    {
        return true;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex)
    {
        Log.errorln("[app_gallery] Failed to create mutex.");
        return false;
    }
    return true;
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0)
    {
        return;
    }

    dst[0] = '\0';
    if (!src || src[0] == '\0')
    {
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void copy_filename(char *dst, size_t dst_size, const char *file_path)
{
    const char *filename = file_path && file_path[0] == '/' ? file_path + 1 : file_path;
    copy_text(dst, dst_size, filename);
}

static int parse_order(const cJSON *image, int fallback_order)
{
    const cJSON *order = cJSON_GetObjectItemCaseSensitive(image, "order");
    if (cJSON_IsNumber(order))
    {
        return order->valueint;
    }
    if (cJSON_IsString(order) && order->valuestring)
    {
        return atoi(order->valuestring);
    }
    return fallback_order;
}

static bool image_is_dynamic(const cJSON *image)
{
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(image, "url");
    if (!cJSON_IsString(url) || !url->valuestring)
    {
        return true;
    }

    String value = url->valuestring;
    int render_pos = value.indexOf("/render/");
    if (render_pos < 0)
    {
        return true;
    }

    int type_start = render_pos + 8;
    int type_end = value.indexOf('/', type_start);
    String type = type_end > type_start ? value.substring(type_start, type_end) : value.substring(type_start);
    return type != "img";
}

static bool image_header_valid(File &file, const char *ext)
{
    if (file.size() < 4 || !file.seek(0))
    {
        return false;
    }

    uint8_t header[8] = {};
    size_t length = file.read(header, sizeof(header));
    if (strcmp(ext, ".bmp") == 0)
    {
        return length >= 2 && header[0] == 'B' && header[1] == 'M';
    }
    if (strcmp(ext, ".png") == 0)
    {
        const uint8_t sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        return length >= sizeof(sig) && memcmp(header, sig, sizeof(sig)) == 0;
    }
    if (strcmp(ext, ".epd") == 0)
    {
        return length >= 4 && header[0] == 'E' && header[1] == 'P' && header[2] == 'D' && header[3] == '0';
    }
    return false;
}

static bool littlefs_image_valid(const String &path, const char *ext)
{
    File file = LittleFS.open(path.c_str(), FILE_READ);
    if (!file)
    {
        return false;
    }
    bool valid = image_header_valid(file, ext);
    file.close();
    return valid;
}

static bool sd_image_valid(const String &path, const char *ext)
{
    File file = HAL::GetHAL().sdOpen(path.c_str(), FILE_READ);
    if (!file)
    {
        return false;
    }
    bool valid = image_header_valid(file, ext);
    file.close();
    return valid;
}

static bool find_local_image(const char *id, bool sd_ready, app_gallery_image_ref_t &image)
{
    const char *exts[] = {".epd", ".bmp", ".png"};

    if (sd_ready)
    {
        HAL::SharedSpiLock lock;
        for (const char *ext : exts)
        {
            String name = String(id) + ext;
            String path = "/" + name;
            if (HAL::GetHAL().sdExists(path.c_str()) && sd_image_valid(path, ext))
            {
                copy_filename(image.filename, sizeof(image.filename), name.c_str());
                image.is_sd = true;
                return true;
            }
        }
    }

    for (const char *ext : exts)
    {
        String name = String(id) + ext;
        String path = "/" + name;
        if (LittleFS.exists(path.c_str()) && littlefs_image_valid(path, ext))
        {
            copy_filename(image.filename, sizeof(image.filename), name.c_str());
            image.is_sd = false;
            return true;
        }
    }

    return false;
}

static bool is_playable(const app_gallery_image_ref_t &image)
{
    return image.id[0] != '\0' && image.filename[0] != '\0';
}

static void sort_images(std::vector<app_gallery_image_ref_t> &images)
{
    std::sort(images.begin(), images.end(), [](const app_gallery_image_ref_t &a, const app_gallery_image_ref_t &b) {
        if (a.order != b.order)
        {
            return a.order < b.order;
        }
        return strcmp(a.id, b.id) < 0;
    });
}

static const app_gallery_image_ref_t *find_current_locked(const char *current_id)
{
    if (!current_id || current_id[0] == '\0')
    {
        return nullptr;
    }

    for (const auto &image : s_images)
    {
        if (is_playable(image) && strcmp(image.id, current_id) == 0)
        {
            return &image;
        }
    }
    return nullptr;
}

static const app_gallery_image_ref_t *first_playable_locked()
{
    for (const auto &image : s_images)
    {
        if (is_playable(image))
        {
            return &image;
        }
    }
    return nullptr;
}

static const app_gallery_image_ref_t *nearest_playable_locked(int order)
{
    const app_gallery_image_ref_t *best = nullptr;
    int best_distance = 0;

    for (const auto &image : s_images)
    {
        if (!is_playable(image))
        {
            continue;
        }

        int distance = abs(image.order - order);
        if (!best || distance < best_distance || (distance == best_distance && image.order > best->order))
        {
            best = &image;
            best_distance = distance;
        }
    }

    return best;
}

static const app_gallery_image_ref_t *select_locked(GalleryMove move)
{
    String current_id = GetCurrentImageId();
    const app_gallery_image_ref_t *current = find_current_locked(current_id.c_str());
    int order = current ? current->order : GetCurrentImageOrder();

    if (move == GalleryMove::Next)
    {
        for (const auto &image : s_images)
        {
            if (is_playable(image) && image.order > order)
            {
                return &image;
            }
        }
        return first_playable_locked();
    }

    for (auto it = s_images.rbegin(); it != s_images.rend(); ++it)
    {
        if (is_playable(*it) && it->order < order)
        {
            return &(*it);
        }
    }

    for (auto it = s_images.rbegin(); it != s_images.rend(); ++it)
    {
        if (is_playable(*it))
        {
            return &(*it);
        }
    }
    return nullptr;
}

static void save_cursor(const app_gallery_image_ref_t &image)
{
    if (!GetCurrentImageId().equals(image.id))
    {
        SetCurrentImageId(image.id);
    }
    if (GetCurrentImageOrder() != image.order)
    {
        SetCurrentImageOrder(image.order);
    }
}

void app_gallery_init()
{
    if (ensure_mutex())
    {
        app_gallery_rebuild_cache_from_manifest();
    }
}

void app_gallery_clear_cache()
{
    if (!ensure_mutex())
    {
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_images.clear();
    s_images.shrink_to_fit();
    xSemaphoreGive(s_mutex);
}

bool app_gallery_has_playable()
{
    if (!ensure_mutex())
    {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool has = first_playable_locked() != nullptr;
    xSemaphoreGive(s_mutex);
    return has;
}

bool app_gallery_rebuild_cache_from_manifest()
{
    File manifest = LittleFS.open("/manifest.json", FILE_READ);
    if (!manifest)
    {
        Log.warningln("[app_gallery] manifest.json not found.");
        return false;
    }

    size_t size = manifest.size();
    if (size == 0 || size > MANIFEST_MAX_SIZE_BYTES)
    {
        Log.warningln("[app_gallery] Invalid manifest size: %u", static_cast<unsigned>(size));
        manifest.close();
        return false;
    }

    char *buffer = static_cast<char *>(ps_malloc(size + 1));
    if (!buffer)
    {
        Log.errorln("[app_gallery] Failed to allocate manifest buffer.");
        manifest.close();
        return false;
    }

    size_t read_size = manifest.readBytes(buffer, size);
    buffer[read_size] = '\0';
    manifest.close();

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    if (!root)
    {
        Log.warningln("[app_gallery] Failed to parse manifest.json.");
        return false;
    }

    const cJSON *images = cJSON_GetObjectItemCaseSensitive(root, "images");
    if (!cJSON_IsArray(images))
    {
        Log.warningln("[app_gallery] Manifest images array missing.");
        cJSON_Delete(root);
        return false;
    }

    std::vector<app_gallery_image_ref_t> cache;
    std::set<String> ids;
    bool sd_ready = HAL::GetHAL().sdEnsureReady();
    int fallback_order = 0;
    const cJSON *item = nullptr;

    cJSON_ArrayForEach(item, images)
    {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (!cJSON_IsString(id) || !id->valuestring || id->valuestring[0] == '\0')
        {
            fallback_order++;
            continue;
        }

        String image_id = id->valuestring;
        if (ids.find(image_id) != ids.end())
        {
            Log.warningln("[app_gallery] Duplicate image id ignored: %s", image_id.c_str());
            fallback_order++;
            continue;
        }
        ids.insert(image_id);

        app_gallery_image_ref_t image = {};
        copy_text(image.id, sizeof(image.id), image_id.c_str());
        image.order = parse_order(item, fallback_order);
        image.is_dynamic = image_is_dynamic(item);
        find_local_image(image.id, sd_ready, image);
        cache.push_back(image);
        fallback_order++;
    }

    cJSON_Delete(root);
    sort_images(cache);

    if (!ensure_mutex())
    {
        return false;
    }

    size_t playable = 0;
    for (const auto &image : cache)
    {
        if (is_playable(image))
        {
            playable++;
        }
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_images.swap(cache);
    size_t item_count = s_images.size();
    xSemaphoreGive(s_mutex);

    app_gallery_image_ref_t current = {};
    app_gallery_current(&current);

    Log.infoln("[app_gallery] Cache rebuilt. items=%u playable=%u",
               static_cast<unsigned>(item_count),
               static_cast<unsigned>(playable));
    return playable > 0;
}

bool app_gallery_mark_available(const char *id, const char *file_path, bool is_sd, bool is_dynamic)
{
    if (!id || id[0] == '\0' || !ensure_mutex())
    {
        return false;
    }

    app_gallery_image_ref_t image = {};
    copy_text(image.id, sizeof(image.id), id);
    copy_filename(image.filename, sizeof(image.filename), file_path);
    image.is_sd = is_sd;
    image.is_dynamic = is_dynamic;
    if (image.filename[0] == '\0')
    {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool updated = false;
    for (auto &cached : s_images)
    {
        if (strcmp(cached.id, id) == 0)
        {
            image.order = cached.order;
            cached = image;
            updated = true;
            break;
        }
    }

    if (!updated)
    {
        image.order = s_images.empty() ? 0 : s_images.back().order + 1;
        s_images.push_back(image);
        sort_images(s_images);
    }
    size_t count = s_images.size();
    xSemaphoreGive(s_mutex);

    app_gallery_image_ref_t current = {};
    app_gallery_current(&current);

    Log.infoln("[app_gallery] Image playable: id=%s file=%s sd=%d dynamic=%d items=%u",
               image.id,
               image.filename,
               image.is_sd ? 1 : 0,
               image.is_dynamic ? 1 : 0,
               static_cast<unsigned>(count));
    return true;
}

bool app_gallery_current(app_gallery_image_ref_t *image)
{
    if (!image || !ensure_mutex())
    {
        return false;
    }

    String current_id = GetCurrentImageId();
    int order = GetCurrentImageOrder();

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const app_gallery_image_ref_t *selected = find_current_locked(current_id.c_str());
    if (!selected)
    {
        selected = nearest_playable_locked(order);
    }
    if (!selected)
    {
        selected = first_playable_locked();
    }

    if (selected)
    {
        *image = *selected;
    }
    xSemaphoreGive(s_mutex);

    if (!selected)
    {
        return false;
    }

    save_cursor(*image);
    return true;
}

static bool app_gallery_select(GalleryMove move, app_gallery_image_ref_t *image)
{
    if (!ensure_mutex())
    {
        return false;
    }

    app_gallery_image_ref_t selected = {};
    bool found = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const app_gallery_image_ref_t *next = select_locked(move);
    if (next)
    {
        selected = *next;
        found = true;
    }
    xSemaphoreGive(s_mutex);

    if (!found)
    {
        Log.warningln("[app_gallery] No playable image for selection.");
        return false;
    }

    save_cursor(selected);
    if (image)
    {
        *image = selected;
    }

    Log.infoln("[app_gallery] %s image selected: id=%s order=%d",
               move == GalleryMove::Next ? "Next" : "Previous",
               selected.id,
               selected.order);
    return true;
}

bool app_gallery_select_next(app_gallery_image_ref_t *image)
{
    return app_gallery_select(GalleryMove::Next, image);
}

bool app_gallery_select_prev(app_gallery_image_ref_t *image)
{
    return app_gallery_select(GalleryMove::Prev, image);
}
