#ifndef APP_GALLERY_H
#define APP_GALLERY_H

#include "app_config.h"

struct app_gallery_image_ref_t
{
    char id[IMAGE_ID_MAX_LEN];
    int order;
    char filename[128];
    bool is_sd;
    bool is_dynamic;
};

void app_gallery_init();
bool app_gallery_rebuild_cache_from_manifest();
void app_gallery_clear_cache();
bool app_gallery_has_playable();
bool app_gallery_mark_available(const char *id, const char *file_path, bool is_sd, bool is_dynamic);
bool app_gallery_current(app_gallery_image_ref_t *image);
bool app_gallery_select_next(app_gallery_image_ref_t *image = nullptr);
bool app_gallery_select_prev(app_gallery_image_ref_t *image = nullptr);

#endif // APP_GALLERY_H
