#pragma once

#include "boards/common/battery_gauge.h"

// Variant definitions for reTerminal E1004 board
static const BatteryCurvePoint BATTERY_CURVE[] = {
    {0.0f, 3.0899f},
    {1.0f, 3.0924f},
    {2.0f, 3.1808f},
    {3.0f, 3.2452f},
    {4.0f, 3.2918f},
    {5.0f, 3.3202f},
    {6.0f, 3.3374f},
    {7.0f, 3.3510f},
    {8.0f, 3.3610f},
    {9.0f, 3.3690f},
    {10.0f, 3.3768f},
    {11.0f, 3.3841f},
    {12.0f, 3.3923f},
    {13.0f, 3.4018f},
    {14.0f, 3.4114f},
    {15.0f, 3.4207f},
    {16.0f, 3.4296f},
    {17.0f, 3.4383f},
    {18.0f, 3.4465f},
    {19.0f, 3.4543f},
    {20.0f, 3.4614f},
    {21.0f, 3.4683f},
    {22.0f, 3.4746f},
    {23.0f, 3.4807f},
    {24.0f, 3.4862f},
    {25.0f, 3.4919f},
    {26.0f, 3.4969f},
    {27.0f, 3.5017f},
    {28.0f, 3.5062f},
    {29.0f, 3.5108f},
    {30.0f, 3.5149f},
    {31.0f, 3.5191f},
    {32.0f, 3.5233f},
    {33.0f, 3.5270f},
    {34.0f, 3.5308f},
    {35.0f, 3.5347f},
    {36.0f, 3.5385f},
    {37.0f, 3.5422f},
    {38.0f, 3.5460f},
    {39.0f, 3.5497f},
    {40.0f, 3.5538f},
    {41.0f, 3.5579f},
    {42.0f, 3.5620f},
    {43.0f, 3.5661f},
    {44.0f, 3.5704f},
    {45.0f, 3.5748f},
    {46.0f, 3.5794f},
    {47.0f, 3.5846f},
    {48.0f, 3.5895f},
    {49.0f, 3.5952f},
    {50.0f, 3.6007f},
    {51.0f, 3.6067f},
    {52.0f, 3.6134f},
    {53.0f, 3.6198f},
    {54.0f, 3.6270f},
    {55.0f, 3.6344f},
    {56.0f, 3.6426f},
    {57.0f, 3.6510f},
    {58.0f, 3.6603f},
    {59.0f, 3.6699f},
    {60.0f, 3.6799f},
    {61.0f, 3.6905f},
    {62.0f, 3.7012f},
    {63.0f, 3.7121f},
    {64.0f, 3.7227f},
    {65.0f, 3.7333f},
    {66.0f, 3.7436f},
    {67.0f, 3.7533f},
    {68.0f, 3.7630f},
    {69.0f, 3.7724f},
    {70.0f, 3.7814f},
    {71.0f, 3.7900f},
    {72.0f, 3.7983f},
    {73.0f, 3.8065f},
    {74.0f, 3.8141f},
    {75.0f, 3.8221f},
    {76.0f, 3.8311f},
    {77.0f, 3.8406f},
    {78.0f, 3.8506f},
    {79.0f, 3.8609f},
    {80.0f, 3.8712f},
    {81.0f, 3.8814f},
    {82.0f, 3.8910f},
    {83.0f, 3.8999f},
    {84.0f, 3.9085f},
    {85.0f, 3.9161f},
    {86.0f, 3.9236f},
    {87.0f, 3.9306f},
    {88.0f, 3.9376f},
    {89.0f, 3.9454f},
    {90.0f, 3.9545f},
    {91.0f, 3.9647f},
    {92.0f, 3.9755f},
    {93.0f, 3.9866f},
    {94.0f, 3.9980f},
    {95.0f, 4.0099f},
    {96.0f, 4.0218f},
    {97.0f, 4.0341f},
    {98.0f, 4.0468f},
    {99.0f, 4.0612f},
    {100.0f, 4.1130f},
};

static const size_t BATTERY_CURVE_SIZE = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);

#define BAT_ADC_PIN 1
#define POWER_EN 46
#define ADC_EN 21
#define GREENLED_PIN 48
#define BUZZER_PIN 45
#define KEY0_PIN 5
#define KEY1_PIN 3
#define KEY2_PIN 4
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
#define UART_TX0 43
#define UART_RX0 44
#define UART_TX1 41
#define UART_RX1 42
#define HEADER_SUPPORT 1
#define HEADER_ADC 6
#define HEADER_GPIO 38
#define HEADER_I2C_SCL ESP32_SCL1
#define HEADER_I2C_SDA ESP32_SDA1
#define HEADER_TX UART_TX1
#define HEADER_RX UART_RX1
