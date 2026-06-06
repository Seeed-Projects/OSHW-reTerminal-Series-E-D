// TouchInput.h -- GT911 capacitive touch driver, click-event flavored.
//
// The reTerminal E1003 carries a GT911-compatible touch controller on the
// same I2C bus that RtcClock already uses (SDA=19, SCL=20). This class
// therefore does NOT call Wire.begin() -- begin() assumes the bus is up.
//
// Public API is intentionally minimal:
//   - begin(intPin, resetPin, displayW, displayH): reset + probe + cache
//     the panel's native coordinate range.
//   - available(): true when the controller was found at boot.
//   - poll(out_x, out_y): edge-detected click event. Returns true exactly
//     once per finger press; subsequent calls with the finger still down
//     return false. The reported coordinates are already mapped to the
//     display's coordinate system.
//
// To swap to a different touch controller, keep this header and only
// rewrite the .cpp.

#ifndef VOICE_MEMO_TOUCH_INPUT_H
#define VOICE_MEMO_TOUCH_INPUT_H

#include <Arduino.h>

class TouchInput {
 public:
  TouchInput();

  bool begin(int intPin, int resetPin, uint16_t displayW, uint16_t displayH);
  bool available() const { return address_ != 0; }

  // Returns true exactly when a new finger-down event was seen since the
  // last poll. *outX / *outY are in display pixels.
  bool poll(uint16_t* outX, uint16_t* outY);

 private:
  static constexpr uint8_t  kAddr1     = 0x5D;
  static constexpr uint8_t  kAddr2     = 0x14;
  static constexpr uint16_t kRegCmd    = 0x8040;
  static constexpr uint16_t kRegProd   = 0x8140;
  static constexpr uint16_t kRegStat   = 0x814E;
  static constexpr uint16_t kRegPoint1 = 0x814F;
  static constexpr uint16_t kRegMaxX   = 0x8048;

  bool read16(uint16_t reg, uint8_t* buf, size_t len);
  bool write16(uint16_t reg, uint8_t value);
  bool probe(uint8_t addr);
  void resetController();
  void readLimits();

  int      intPin_;
  int      resetPin_;
  uint8_t  address_;
  uint16_t rawMaxX_;
  uint16_t rawMaxY_;
  uint16_t displayW_;
  uint16_t displayH_;

  bool          pressed_;       // true while a finger is currently down
  unsigned long lastPollMs_;
};

#endif  // VOICE_MEMO_TOUCH_INPUT_H
