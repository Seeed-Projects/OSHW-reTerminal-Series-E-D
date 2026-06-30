#pragma once

#if defined(RETERMINAL_E1001)
#include "driver_e1001.h"
#elif defined(RETERMINAL_E1002)
#include "driver_e1002.h"
#elif defined(RETERMINAL_E1003)
#include "driver_e1003.h"
#elif defined(RETERMINAL_E1004)
#include "driver_e1004.h"
#else
#error "Select a supported PlatformIO environment: reterminal_e1001, reterminal_e1002, reterminal_e1003, or reterminal_e1004."
#endif
