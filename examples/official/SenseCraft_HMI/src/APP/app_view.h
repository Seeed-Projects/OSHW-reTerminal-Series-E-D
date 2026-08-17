#ifndef VIEW_H
#define VIEW_H

#include <Arduino.h>

void app_view_init();

bool imgBufferMutex_lock();
void imgBufferMutex_unlock();
bool app_view_is_showing_activation();
bool app_view_is_refreshing();

#endif // VIEW_H
