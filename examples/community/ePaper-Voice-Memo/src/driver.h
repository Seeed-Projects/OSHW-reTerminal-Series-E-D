#ifndef VOICE_MEMO_REMINDER_DRIVER_H
#define VOICE_MEMO_REMINDER_DRIVER_H

// The device target is selected by platformio.ini via a build flag:
//   -D VOICE_MEMO_DEVICE_E1001 / E1002 / E1003
// E1004 is intentionally unsupported because it has no onboard microphone.
// Fall back to E1003 when built without an explicit target (e.g. Arduino IDE).
#if !defined(VOICE_MEMO_DEVICE_E1001) && \
    !defined(VOICE_MEMO_DEVICE_E1002) && \
    !defined(VOICE_MEMO_DEVICE_E1003)
  #define VOICE_MEMO_DEVICE_E1003
#endif

#define VM_SCREEN_GRAY4   1
#define VM_SCREEN_COLOR6  2
#define VM_SCREEN_GRAY16  3

#if defined(VOICE_MEMO_DEVICE_E1001)
  #define BOARD_SCREEN_COMBO 520
  #define VM_DEVICE_NAME "reTerminal E1001"
  #define VM_SCREEN_MODE VM_SCREEN_GRAY4
  #define VM_LED_PIN 6
  #define VM_HAS_TOUCH 0
  #define VM_VISIBLE_MEMO_MAX 4
#elif defined(VOICE_MEMO_DEVICE_E1002)
  #define BOARD_SCREEN_COMBO 521
  #define VM_DEVICE_NAME "reTerminal E1002"
  #define VM_SCREEN_MODE VM_SCREEN_COLOR6
  #define VM_LED_PIN 6
  #define VM_HAS_TOUCH 0
  #define VM_VISIBLE_MEMO_MAX 4
#elif defined(VOICE_MEMO_DEVICE_E1003)
  #define BOARD_SCREEN_COMBO 522
  #define VM_DEVICE_NAME "reTerminal E1003"
  #define VM_SCREEN_MODE VM_SCREEN_GRAY16
  #define VM_LED_PIN 16
  #define VM_HAS_TOUCH 1
  #define VM_VISIBLE_MEMO_MAX 8
#else
  #error "Select VOICE_MEMO_DEVICE_E1001, VOICE_MEMO_DEVICE_E1002, or VOICE_MEMO_DEVICE_E1003."
#endif

#endif
