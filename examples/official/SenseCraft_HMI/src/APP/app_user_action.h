#pragma once

#include "hal/hal_indicator.h"

void app_user_action_mark_pending();
void app_user_action_mark_refresh_pending(bool completion_tone);
void app_user_action_begin(hal_indicator_pattern_t tone = HAL_INDICATOR_CLICK);
void app_user_action_begin_refresh(bool completion_tone);
void app_user_action_finish_refresh();
bool app_user_action_is_active();
