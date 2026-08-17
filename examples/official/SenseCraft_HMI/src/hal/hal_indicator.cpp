#include "hal/hal_indicator.h"

#include "app_config.h"
#include "hal/hal.h"

#include <Arduino.h>
#include "ArduinoLog.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace
{
constexpr UBaseType_t INDICATOR_QUEUE_SIZE = 8;
constexpr uint32_t INDICATOR_TASK_STACK_SIZE = 2048;

QueueHandle_t g_indicator_queue = nullptr;
TaskHandle_t g_indicator_task = nullptr;

void pulse_led(uint32_t on_ms, uint32_t off_ms = 0)
{
    HAL::GetHAL().ledSet(true);
    delay(on_ms);
    HAL::GetHAL().ledSet(false);
    if (off_ms > 0)
    {
        delay(off_ms);
    }
}

void beep_or_pulse(uint32_t frequency_hz, uint32_t duration_ms, uint32_t off_ms = 0)
{
    if (HAL::GetHAL().buzzerBeep(frequency_hz, duration_ms))
    {
        delay(duration_ms);
        HAL::GetHAL().buzzerStop();
        if (off_ms > 0)
        {
            delay(off_ms);
        }
        return;
    }

    pulse_led(duration_ms, off_ms);
}

void play_pattern(hal_indicator_pattern_t pattern)
{
    switch (pattern)
    {
    case HAL_INDICATOR_CLICK:
        beep_or_pulse(1319, 35);
        break;

    case HAL_INDICATOR_PRIMARY_ACTION:
        beep_or_pulse(1760, 45);
        break;

    case HAL_INDICATOR_IMAGE_DOWNLOAD:
        pulse_led(100);
        break;

    case HAL_INDICATOR_REFRESH_DONE:
        beep_or_pulse(988, 45, 20);
        beep_or_pulse(1319, 55, 20);
        beep_or_pulse(1568, 70);
        break;

    case HAL_INDICATOR_ERROR:
        beep_or_pulse(392, 150, 50);
        beep_or_pulse(262, 150);
        break;
    }
}

void indicator_task(void *)
{
    hal_indicator_pattern_t pattern;
    while (true)
    {
        if (xQueueReceive(g_indicator_queue, &pattern, portMAX_DELAY) == pdTRUE)
        {
            play_pattern(pattern);
        }
    }
}
}

void hal_indicator_init()
{
    if (g_indicator_task != nullptr)
    {
        return;
    }

    g_indicator_queue = xQueueCreate(INDICATOR_QUEUE_SIZE, sizeof(hal_indicator_pattern_t));
    if (g_indicator_queue == nullptr)
    {
        Log.errorln("[hal_indicator] Failed to create indicator queue.");
        return;
    }

    BaseType_t ok = xTaskCreate(
        indicator_task,
        "hal_indicator",
        INDICATOR_TASK_STACK_SIZE,
        nullptr,
        tskIDLE_PRIORITY + 1,
        &g_indicator_task);
    if (ok != pdPASS)
    {
        Log.errorln("[hal_indicator] Failed to create indicator task.");
        vQueueDelete(g_indicator_queue);
        g_indicator_queue = nullptr;
        g_indicator_task = nullptr;
    }
}

void hal_indicator_play(hal_indicator_pattern_t pattern)
{
    if (g_indicator_queue == nullptr)
    {
        return;
    }

    xQueueSend(g_indicator_queue, &pattern, 0);
}
