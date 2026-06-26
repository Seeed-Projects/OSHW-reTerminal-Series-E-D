#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_icon;
extern const lv_img_dsc_t img___;
extern const lv_img_dsc_t img___1;
extern const lv_img_dsc_t img___2;
extern const lv_img_dsc_t img___3;
extern const lv_img_dsc_t img___4;
extern const lv_img_dsc_t img___5;
extern const lv_img_dsc_t img___6;
extern const lv_img_dsc_t img___11;
extern const lv_img_dsc_t img___21;
extern const lv_img_dsc_t img___7;
extern const lv_img_dsc_t img_____;
extern const lv_img_dsc_t img_____1;
extern const lv_img_dsc_t img__;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[14];


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/