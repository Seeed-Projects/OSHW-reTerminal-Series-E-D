#pragma once

#include "boards/common/battery_gauge.h"

// Variant definitions for reTerminal E1003 board
static const BatteryCurvePoint BATTERY_CURVE[] = {
    {0.0f, 3.204f},
    {1.0f, 3.2628f},
    {2.0f, 3.3085f},
    {3.0f, 3.3454f},
    {4.0f, 3.3758f},
    {5.0f, 3.4016f},
    {6.0f, 3.4240f},
    {7.0f, 3.4438f},
    {8.0f, 3.4617f},
    {9.0f, 3.4772f},
    {10.0f, 3.4916f},
    {11.0f, 3.5048f},
    {12.0f, 3.5171f},
    {13.0f, 3.5285f},
    {14.0f, 3.5394f},
    {15.0f, 3.5498f},
    {16.0f, 3.5597f},
    {17.0f, 3.5696f},
    {18.0f, 3.5787f},
    {19.0f, 3.5879f},
    {20.0f, 3.5969f},
    {21.0f, 3.6056f},
    {22.0f, 3.6141f},
    {23.0f, 3.6226f},
    {24.0f, 3.6312f},
    {25.0f, 3.6396f},
    {26.0f, 3.6479f},
    {27.0f, 3.6563f},
    {28.0f, 3.6648f},
    {29.0f, 3.6734f},
    {30.0f, 3.6821f},
    {31.0f, 3.6909f},
    {32.0f, 3.6998f},
    {33.0f, 3.7086f},
    {34.0f, 3.7178f},
    {35.0f, 3.7270f},
    {36.0f, 3.7362f},
    {37.0f, 3.7454f},
    {38.0f, 3.7545f},
    {39.0f, 3.7636f},
    {40.0f, 3.7724f},
    {41.0f, 3.7811f},
    {42.0f, 3.7897f},
    {43.0f, 3.7981f},
    {44.0f, 3.8059f},
    {45.0f, 3.8138f},
    {46.0f, 3.8214f},
    {47.0f, 3.8286f},
    {48.0f, 3.8357f},
    {49.0f, 3.8426f},
    {50.0f, 3.8493f},
    {51.0f, 3.8557f},
    {52.0f, 3.8621f},
    {53.0f, 3.8681f},
    {54.0f, 3.8740f},
    {55.0f, 3.8797f},
    {56.0f, 3.8850f},
    {57.0f, 3.8902f},
    {58.0f, 3.8954f},
    {59.0f, 3.9003f},
    {60.0f, 3.9050f},
    {61.0f, 3.9099f},
    {62.0f, 3.9145f},
    {63.0f, 3.9190f},
    {64.0f, 3.9234f},
    {65.0f, 3.9279f},
    {66.0f, 3.9322f},
    {67.0f, 3.9366f},
    {68.0f, 3.9408f},
    {69.0f, 3.9452f},
    {70.0f, 3.9493f},
    {71.0f, 3.9535f},
    {72.0f, 3.9576f},
    {73.0f, 3.9619f},
    {74.0f, 3.9665f},
    {75.0f, 3.9709f},
    {76.0f, 3.9751f},
    {77.0f, 3.9797f},
    {78.0f, 3.9843f},
    {79.0f, 3.9889f},
    {80.0f, 3.9939f},
    {81.0f, 3.9990f},
    {82.0f, 4.0042f},
    {83.0f, 4.0095f},
    {84.0f, 4.0149f},
    {85.0f, 4.0206f},
    {86.0f, 4.0265f},
    {87.0f, 4.0326f},
    {88.0f, 4.0388f},
    {89.0f, 4.0450f},
    {90.0f, 4.0509f},
    {91.0f, 4.0567f},
    {92.0f, 4.0629f},
    {93.0f, 4.0689f},
    {94.0f, 4.0749f},
    {95.0f, 4.0809f},
    {96.0f, 4.0868f},
    {97.0f, 4.0928f},
    {98.0f, 4.0985f},
    {99.0f, 4.1042f},
    {100.0f, 4.1100f},
};

static const size_t BATTERY_CURVE_SIZE = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);

#define BAT_ADC_PIN 1
#define POWER_EN 40
#define PDM_MIC_EN_PIN 38
#define PDM_MIC_DATA_PIN 41
#define PDM_MIC_CLOCK_PIN 42
#define UART_TX0 43
#define UART_RX0 44
#define UART_TX1 17
#define UART_RX1 18
#define GREENLED_PIN 16
#define BUZZER_PIN 45
#define KEY0_PIN 3
#define KEY1_PIN 4
#define KEY2_PIN 5
#define SD_EN_PIN 39
#define SD_DET_PIN 15
#define SD_CS_PIN 14
#define SD_MOSI_PIN 9
#define SD_MISO_PIN 8
#define SD_SCK_PIN 7
#define ESP32_SCL 20
#define ESP32_SDA 19
#define TOUCH_SCL ESP32_SCL
#define TOUCH_SDA ESP32_SDA
#define TOUCH_INT 2
#define TOUCH_RST 48
#define HEADER_SUPPORT 1
#define HEADER_EN 46
#define HEADER_ADC1_CH_1 2
#define HEADER_ADC1_CH_2 6
#define HEADER_I2C_SCL ESP32_SCL
#define HEADER_I2C_SDA ESP32_SDA
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
