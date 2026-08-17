#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cJSON.h"

#include "utils/util.h"

int wifi_rssi_level_get(int rssi)
{
    //    0    rssi<=-100
    //    1    (-100, -88]
    //    2    (-88, -77]
    //    3    (-66, -55]
    //    4    rssi>=-55
    if( rssi > -55 ) {
        return 4;
    } else if( rssi > -66 ) {
        return 3;
    } else if( rssi > -88) {
        return 2;
    } else {
        return 1;
    }
}

time_t util_get_timestamp_ms(void)
{
    time_t now_ms;

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    now_ms = (time_t)((int64_t)tv_now.tv_sec * 1000 + (int64_t)tv_now.tv_usec/1000);

    return now_ms;
}

void byte_array_to_hex_string(const uint8_t *byteArray, size_t byteArraySize, char *hexString)
{
    for (size_t i = 0; i < byteArraySize; ++i)
    {
        sprintf(&hexString[2 * i], "%02X", byteArray[i]);
    }
}

void string_to_byte_array(const char *str, uint8_t *byte_array, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        sscanf(str + 2 * i, "%2hhx", &byte_array[i]);
    }
}

void *psram_malloc(size_t sz)
{
    return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
}

void *psram_calloc(size_t n, size_t sz)
{
    return heap_caps_calloc(n, sz, MALLOC_CAP_SPIRAM);
}

void *psram_realloc(void *ptr, size_t new_sz)
{
    return heap_caps_realloc(ptr, new_sz, MALLOC_CAP_SPIRAM);
}



bool cJSON_IsGeneralBool(const cJSON * const item)
{
    return cJSON_IsBool(item) || cJSON_IsNumber(item);
}

bool cJSON_IsGeneralTrue(const cJSON * const item)
{
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
    else if (cJSON_IsNumber(item)) return (item->valueint != 0);
    else return false;
}

char *strdup_psram(const char *s)
{
    size_t len = strlen(s) + 1;
    void *new = heap_caps_calloc(1, len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (new == NULL)
        return NULL;
    return (char *)memcpy(new, s, len);
}
