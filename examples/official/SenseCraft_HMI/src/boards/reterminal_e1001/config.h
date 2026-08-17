#pragma once

#include "boards/common/battery_gauge.h"

// Variant definitions for reTerminal E1001/E1002 boards
static const BatteryCurvePoint BATTERY_CURVE[] = {
    {0.0f, 2.795f},
    {1.0f, 2.795f},
    {2.0f, 2.990f},
    {3.0f, 3.107f},
    {4.0f, 3.189f},
    {5.0f, 3.252f},
    {6.0f, 3.300f},
    {7.0f, 3.334f},
    {8.0f, 3.354f},
    {9.0f, 3.367f},
    {10.0f, 3.378f},
    {11.0f, 3.389f},
    {12.0f, 3.399f},
    {13.0f, 3.409f},
    {14.0f, 3.418f},
    {15.0f, 3.426f},
    {16.0f, 3.434f},
    {17.0f, 3.442f},
    {18.0f, 3.449f},
    {19.0f, 3.457f},
    {20.0f, 3.464f},
    {21.0f, 3.471f},
    {22.0f, 3.478f},
    {23.0f, 3.485f},
    {24.0f, 3.491f},
    {25.0f, 3.498f},
    {26.0f, 3.504f},
    {27.0f, 3.510f},
    {28.0f, 3.516f},
    {29.0f, 3.522f},
    {30.0f, 3.528f},
    {31.0f, 3.534f},
    {32.0f, 3.540f},
    {33.0f, 3.546f},
    {34.0f, 3.552f},
    {35.0f, 3.558f},
    {36.0f, 3.564f},
    {37.0f, 3.569f},
    {38.0f, 3.575f},
    {39.0f, 3.581f},
    {40.0f, 3.587f},
    {41.0f, 3.592f},
    {42.0f, 3.598f},
    {43.0f, 3.603f},
    {44.0f, 3.609f},
    {45.0f, 3.614f},
    {46.0f, 3.620f},
    {47.0f, 3.625f},
    {48.0f, 3.631f},
    {49.0f, 3.636f},
    {50.0f, 3.642f},
    {51.0f, 3.647f},
    {52.0f, 3.653f},
    {53.0f, 3.658f},
    {54.0f, 3.664f},
    {55.0f, 3.669f},
    {56.0f, 3.675f},
    {57.0f, 3.680f},
    {58.0f, 3.686f},
    {59.0f, 3.691f},
    {60.0f, 3.697f},
    {61.0f, 3.703f},
    {62.0f, 3.709f},
    {63.0f, 3.715f},
    {64.0f, 3.721f},
    {65.0f, 3.727f},
    {66.0f, 3.733f},
    {67.0f, 3.739f},
    {68.0f, 3.746f},
    {69.0f, 3.752f},
    {70.0f, 3.759f},
    {71.0f, 3.765f},
    {72.0f, 3.772f},
    {73.0f, 3.779f},
    {74.0f, 3.786f},
    {75.0f, 3.793f},
    {76.0f, 3.801f},
    {77.0f, 3.808f},
    {78.0f, 3.816f},
    {79.0f, 3.824f},
    {80.0f, 3.832f},
    {81.0f, 3.840f},
    {82.0f, 3.849f},
    {83.0f, 3.858f},
    {84.0f, 3.867f},
    {85.0f, 3.876f},
    {86.0f, 3.886f},
    {87.0f, 3.896f},
    {88.0f, 3.906f},
    {89.0f, 3.916f},
    {90.0f, 3.927f},
    {91.0f, 3.984f},
    {92.0f, 3.995f},
    {93.0f, 4.007f},
    {94.0f, 4.019f},
    {95.0f, 4.032f},
    {96.0f, 4.045f},
    {97.0f, 4.059f},
    {98.0f, 4.074f},
    {99.0f, 4.090f},
    {100.0f, 4.111f},
};

static const size_t BATTERY_CURVE_SIZE = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);

#define BAT_ADC_PIN 1
#define ADC_EN 21
#define GREENLED_PIN 6
#define BUZZER_PIN 45
#define KEY0_PIN 3
#define KEY1_PIN 4
#define KEY2_PIN 5
#define UART_TX0 43
#define UART_RX0 44
#define SD_EN_PIN 16
#define SD_DET_PIN 15
#define SD_CS_PIN 14
#define SD_MOSI_PIN 9
#define SD_MISO_PIN 8
#define SD_SCK_PIN 7
#define ESP32_SCL 20
#define ESP32_SDA 19
#define ESP32_SCL1 40
#define ESP32_SDA1 39
#define TOUCH_SCL ESP32_SCL
#define TOUCH_SDA ESP32_SDA
#define TOUCH_INT 47
#define TOUCH_RST 48
#define PMIC_I2C_SCL ESP32_SCL1
#define PMIC_I2C_SDA ESP32_SDA1
#define PMIC_I2C_ADDRESS 0x6B
#define PMIC_REG_CHG_CURRENT 0x04
#define PMIC_REG_CHG_SHIFT 0
#define PMIC_REG_CHG_MASK 0x3F
#define PMIC_CHG_STEP_MA 60
#define PMIC_CHG_OFFSET_MA 0
#define PMIC_CHG_MIN_MA 0
#define PMIC_CHG_MAX_MA 3000
