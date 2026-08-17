#include "APP/app_input.h"

#include <Arduino.h>
#include "ArduinoLog.h"

#include "hal/hal.h"
#include "APP/app_power_manager.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

ESP_EVENT_DEFINE_BASE(APP_INPUT_EVENT_BASE);

static constexpr uint32_t INPUT_TASK_STACK_SIZE = 3072;
static constexpr UBaseType_t INPUT_TASK_PRIORITY = tskIDLE_PRIORITY + 3;
static constexpr TickType_t INPUT_TASK_INTERVAL = pdMS_TO_TICKS(20);
static constexpr TickType_t INPUT_EVENT_POST_TIMEOUT = pdMS_TO_TICKS(50);

static Button* button1 = nullptr;
static Button* button2 = nullptr;
static Button* button3 = nullptr;

static volatile bool button2LongPressActive = false;
static volatile bool button3LongPressActive = false;

static void __handle_dual_long_press();

static bool wait_release_btn1 = false;
static bool wait_release_btn2 = false;
static bool wait_release_btn3 = false;

static bool __post_input_event(app_input_event_id_t event_id, app_input_source_t source)
{
    app_power_manager_activity(AppPowerOwner::UserAction);

    app_input_event_data_t data = {
        .source = source,
    };
    esp_err_t err = esp_event_post(APP_INPUT_EVENT_BASE, event_id, &data, sizeof(data), INPUT_EVENT_POST_TIMEOUT);
    if (err != ESP_OK)
    {
        Log.errorln("[app_input] Failed to post input event: id=%d err=%d", event_id, err);
        return false;
    }
    return true;
}

static void __handle_click1()
{
    Log.verboseln("[app_input] button 1 click");
    __post_input_event(APP_INPUT_EVENT_PRIMARY_ACTION, APP_INPUT_SOURCE_KEY0);
}

static void __handle_click2()
{
    Log.verboseln("[app_input] button 2 click");
    __post_input_event(APP_INPUT_EVENT_NEXT_IMAGE, APP_INPUT_SOURCE_KEY1);
}

static void __handle_click3()
{
    Log.verboseln("[app_input] button 3 click");
    __post_input_event(APP_INPUT_EVENT_PREV_IMAGE, APP_INPUT_SOURCE_KEY2);
}

static void __long_press1_start()
{
    Log.verboseln("[app_input] button 1 long press start");
    __post_input_event(APP_INPUT_EVENT_CLEAR_SCREEN, APP_INPUT_SOURCE_KEY0);
}

static void __long_press2_start()
{
    Log.verboseln("[app_input] button 2 long press start");
    button2LongPressActive = true;
    if (button3LongPressActive)
    {
        __handle_dual_long_press();
    }
}

static void __long_press2_stop()
{
    button2LongPressActive = false;
    Log.verboseln("[app_input] button 2 long press stop");
}

static void __long_press3_start()
{
    Log.verboseln("[app_input] button 3 long press start");
    button3LongPressActive = true;
    if (button2LongPressActive)
    {
        __handle_dual_long_press();
    }
}

static void __long_press3_stop()
{
    button3LongPressActive = false;
    Log.verboseln("[app_input] button 3 long press stop");
}

static void __handle_dual_long_press()
{
    Log.verboseln("[app_input] buttons 2 and 3 long pressed");
    __post_input_event(APP_INPUT_EVENT_ENTER_PROVISIONING, APP_INPUT_SOURCE_KEY1_KEY2);
}

static void __configure_button_events()
{
    button1 = HAL::GetHAL().button(0);
    button2 = HAL::GetHAL().button(1);
    button3 = HAL::GetHAL().button(2);

    if (button1) {
        button1->onClick(__handle_click1);
        button1->onLongPressStart(__long_press1_start);
        button1->setPressMs(3000);
    }

    if (button2) {
        button2->onClick(__handle_click2);
        button2->onLongPressStart(__long_press2_start);
        button2->onLongPressStop(__long_press2_stop);
    }

    if (button3) {
        button3->onClick(__handle_click3);
        button3->onLongPressStart(__long_press3_start);
        button3->onLongPressStop(__long_press3_stop);
    }
}

static void __button_loop()
{
    // Skip processing buttons that were already held during startup until they are released once.
    if (button1 && wait_release_btn1)
    {
        if (!button1->isPressed())
        {
            wait_release_btn1 = false;
            button1->reset();
        }
    }
    else if (button1)
    {
        button1->tick();
    }

    if (button2 && wait_release_btn2)
    {
        if (!button2->isPressed())
        {
            wait_release_btn2 = false;
            button2->reset();
        }
    }
    else if (button2)
    {
        button2->tick();
    }

    if (button3 && wait_release_btn3)
    {
        if (!button3->isPressed())
        {
            wait_release_btn3 = false;
            button3->reset();
        }
    }
    else if (button3)
    {
        button3->tick();
    }
}

namespace
{
    void input_task(void *)
    {
        while (true)
        {
            __button_loop();
            HAL::GetHAL().touchLoop();
            vTaskDelay(INPUT_TASK_INTERVAL);
        }
    }

    bool g_input_task_registered = false;
    TaskHandle_t g_input_task_handle = nullptr;
}

void app_input_init()
{
    __configure_button_events();

    HAL::GetHAL().touchOnGesture([](const GTGestureEvent &event) {
        switch (event.type)
        {
        case GTGesture::SwipeLeft:
            Log.verboseln("[app_input] gesture swipe left");
            __post_input_event(APP_INPUT_EVENT_NEXT_IMAGE, APP_INPUT_SOURCE_TOUCH);
            
            break;
        case GTGesture::SwipeRight:
            Log.verboseln("[app_input] gesture swipe right");
            __post_input_event(APP_INPUT_EVENT_PREV_IMAGE, APP_INPUT_SOURCE_TOUCH);
            break;
        case GTGesture::DoubleTap:
            Log.verboseln("[app_input] gesture double tap");
            __post_input_event(APP_INPUT_EVENT_PRIMARY_ACTION, APP_INPUT_SOURCE_TOUCH);
            break;
        default:
            break;
        }
    });

    // If a button is already held when the app starts, ignore it until it is released once.
    wait_release_btn1 = button1 && button1->isPressed();
    wait_release_btn2 = button2 && button2->isPressed();
    wait_release_btn3 = button3 && button3->isPressed();

    if (!g_input_task_registered)
    {
        BaseType_t task_created = xTaskCreate(
            input_task,
            "app_input",
            INPUT_TASK_STACK_SIZE,
            nullptr,
            INPUT_TASK_PRIORITY,
            &g_input_task_handle);
        if (task_created != pdPASS)
        {
            g_input_task_handle = nullptr;
            Log.errorln("[app_input] Failed to create input task");
        }
        else
        {
            g_input_task_registered = true;
        }
    }
}
