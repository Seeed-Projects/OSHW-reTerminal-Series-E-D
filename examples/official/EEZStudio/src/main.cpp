#include <Arduino.h>
#include <lvgl.h>

#include "epaper_driver.h"
#include "ui/ui.h"

#define SERIAL_RX 44
#define SERIAL_TX 43

#if BOARD_SCREEN_COMBO == 520
  #define BOARD_NAME "E1001 (7.5\" BW 800x480)"
#elif BOARD_SCREEN_COMBO == 521
  #define BOARD_NAME "E1002 (7.5\" Color 800x480)"
#elif BOARD_SCREEN_COMBO == 522
  #define BOARD_NAME "E1003 (10.3\" BW 1872x1404)"
#elif BOARD_SCREEN_COMBO == 523
  #define BOARD_NAME "E1004 (13.3\" Color 1200x1600)"
#else
  #define BOARD_NAME "Unknown"
#endif

void setup() {
    Serial1.begin(115200, SERIAL_8N1, SERIAL_RX, SERIAL_TX);
    delay(2500);
    Serial1.println("\n========================================");
    Serial1.printf("reTerminal %s\n", BOARD_NAME);
    Serial1.println("EEZ Studio LVGL UI");
    Serial1.println("========================================");

    Serial1.printf("[INIT] Free heap: %lu bytes, PSRAM: %lu bytes\n",
                  (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getFreePsram());

    Serial1.println("[INIT] Calling lv_init()...");
    lv_init();
    Serial1.println("[INIT] lv_init() done.");

    Serial1.println("[INIT] Calling epaper_init()...");
    epaper_init();
    Serial1.println("[INIT] epaper_init() done.");

    Serial1.printf("[INIT] Free heap after epaper_init: %lu bytes\n",
                  (unsigned long)ESP.getFreeHeap());

    Serial1.println("[INIT] Calling ui_init()...");
    ui_init();
    Serial1.println("[INIT] ui_init() done.");

    Serial1.println("[INIT] Active screen pointer check...");
    lv_obj_t *scr = lv_screen_active();
    Serial1.printf("[INIT] Active screen: %p\n", (void *)scr);
    if (scr) {
        int32_t child_cnt = lv_obj_get_child_count(scr);
        Serial1.printf("[INIT] Active screen has %ld children\n", (long)child_cnt);
    }

    Serial1.println("[INIT] Calling lv_timer_handler() #1...");
    lv_timer_handler();
    Serial1.println("[INIT] lv_timer_handler() #1 done.");

    Serial1.println("[INIT] Waiting 500ms for any pending animation...");
    delay(500);

    Serial1.println("[INIT] Calling lv_timer_handler() #2...");
    lv_timer_handler();
    Serial1.println("[INIT] lv_timer_handler() #2 done.");

    Serial1.println("[INIT] Forcing full refresh via epaper_refresh()...");
    epaper_refresh();
    Serial1.println("[INIT] epaper_refresh() done.");

    Serial1.println("[INIT] Setup complete.");
    Serial1.printf("[INIT] Free heap: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
}

void loop() {
    lv_timer_handler();
    ui_tick();
    delay(100);
}
