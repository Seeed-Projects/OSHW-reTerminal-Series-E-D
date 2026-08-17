
#ifndef _UTIL_H
#define _UTIL_H

#include <time.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

int wifi_rssi_level_get(int rssi);

time_t util_get_timestamp_ms(void);

void byte_array_to_hex_string(const uint8_t *byteArray, size_t byteArraySize, char *hexString);
void string_to_byte_array(const char *str, uint8_t *byte_array, size_t length);

void *psram_malloc(size_t sz);
void *psram_calloc(size_t n, size_t sz);
void *psram_realloc(void *ptr, size_t new_sz);



bool cJSON_IsGeneralBool(const cJSON * const item);
bool cJSON_IsGeneralTrue(const cJSON * const item);

char *strdup_psram(const char *s);

#ifdef __cplusplus
}
#endif

#endif
