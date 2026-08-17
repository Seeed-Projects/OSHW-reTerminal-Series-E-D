#pragma once

enum hal_indicator_pattern_t
{
    HAL_INDICATOR_CLICK,
    HAL_INDICATOR_PRIMARY_ACTION,
    HAL_INDICATOR_IMAGE_DOWNLOAD,
    HAL_INDICATOR_REFRESH_DONE,
    HAL_INDICATOR_ERROR,
};

void hal_indicator_init();
void hal_indicator_play(hal_indicator_pattern_t pattern);
