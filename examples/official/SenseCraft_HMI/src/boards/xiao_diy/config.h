#pragma once

#include "boards/common/battery_gauge.h"

// Default variant definitions when no specific board is selected
static const BatteryCurvePoint BATTERY_CURVE[] = {
    {0.0f, 2.910f},
    {2.0f, 3.060f},
    {3.0f, 3.150f},
    {4.0f, 3.220f},
    {5.0f, 3.280f},
    {6.0f, 3.320f},
    {7.0f, 3.360f},
    {8.0f, 3.390f},
    {9.0f, 3.420f},
    {10.0f, 3.450f},
    {12.0f, 3.500f},
    {14.0f, 3.540f},
    {16.0f, 3.570f},
    {18.0f, 3.600f},
    {20.0f, 3.630f},
    {25.0f, 3.690f},
    {30.0f, 3.740f},
    {35.0f, 3.790f},
    {40.0f, 3.830f},
    {45.0f, 3.870f},
    {50.0f, 3.910f},
    {55.0f, 3.940f},
    {60.0f, 3.980f},
    {65.0f, 4.010f},
    {70.0f, 4.040f},
    {75.0f, 4.070f},
    {80.0f, 4.090f},
    {85.0f, 4.120f},
    {90.0f, 4.140f},
    {100.0f, 4.150f},
};

static const size_t BATTERY_CURVE_SIZE = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);

#define BAT_ADC_PIN 1
#define POWER_EN 43
#define ADC_EN 6
#define GREENLED_PIN 21
#ifdef USE_XIAO_EPAPER_DISPLAY_BOARD_EE05
#define KEY0_PIN 2
#define KEY1_PIN 3
#define KEY2_PIN 8
#else
#define KEY0_PIN 2
#define KEY1_PIN 3
#define KEY2_PIN 5
#endif
#define ESP32_SCL 40
#define ESP32_SDA 41
#define PMIC_I2C_SCL ESP32_SCL
#define PMIC_I2C_SDA ESP32_SDA
#define PMIC_I2C_ADDRESS 0x6B
#define PMIC_REG_CHG_CURRENT 0x04
#define PMIC_REG_CHG_SHIFT 0
#define PMIC_REG_CHG_MASK 0x3F
#define PMIC_CHG_STEP_MA 60
#define PMIC_CHG_OFFSET_MA 0
#define PMIC_CHG_MIN_MA 0
#define PMIC_CHG_MAX_MA 3000

#define SD_EN_PIN -1
#define SD_DET_PIN -1
#define SD_CS_PIN 21
#define SD_MOSI_PIN 9
#define SD_MISO_PIN 8
#define SD_SCK_PIN 7
