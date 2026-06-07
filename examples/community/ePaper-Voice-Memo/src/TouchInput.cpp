#include "TouchInput.h"

#include <Wire.h>

#include "TouchMapper.h"

namespace {
constexpr unsigned long kPollMs = 30;
}

TouchInput::TouchInput()
  : intPin_(-1),
    resetPin_(-1),
    address_(0),
    rawMaxX_(1),
    rawMaxY_(1),
    displayW_(1),
    displayH_(1),
    pressed_(false),
    lastPollMs_(0)
{
}

bool TouchInput::read16(uint16_t reg, uint8_t* buf, size_t len)
{
  Wire.beginTransmission(address_);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;

  const uint8_t got = Wire.requestFrom(address_, static_cast<uint8_t>(len));
  if (got != len) return false;

  for (size_t i = 0; i < len; i++) {
    buf[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool TouchInput::write16(uint16_t reg, uint8_t value)
{
  Wire.beginTransmission(address_);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void TouchInput::resetController()
{
  pinMode(intPin_, INPUT);
  pinMode(resetPin_, OUTPUT);
  digitalWrite(resetPin_, LOW);
  delay(20);
  digitalWrite(resetPin_, HIGH);
  delay(120);
}

bool TouchInput::probe(uint8_t addr)
{
  address_ = addr;
  uint8_t product[4] = {};
  if (!read16(kRegProd, product, sizeof(product))) {
    address_ = 0;
    return false;
  }
  Serial1.printf("[touch] GT9xx at 0x%02X, product: %c%c%c%c\n",
                 addr, product[0], product[1], product[2], product[3]);
  return true;
}

void TouchInput::readLimits()
{
  uint8_t raw[4] = {};
  if (!read16(kRegMaxX, raw, sizeof(raw))) {
    rawMaxX_ = displayW_;
    rawMaxY_ = displayH_;
    return;
  }
  const uint16_t mx = static_cast<uint16_t>(raw[0] | (raw[1] << 8));
  const uint16_t my = static_cast<uint16_t>(raw[2] | (raw[3] << 8));
  if (mx > 0 && my > 0) {
    rawMaxX_ = mx;
    rawMaxY_ = my;
  }
  Serial1.printf("[touch] range=%ux%u, display=%ux%u\n",
                 rawMaxX_, rawMaxY_, displayW_, displayH_);
}

bool TouchInput::begin(int intPin, int resetPin,
                       uint16_t displayW, uint16_t displayH)
{
  intPin_   = intPin;
  resetPin_ = resetPin;
  displayW_ = displayW ? displayW : 1;
  displayH_ = displayH ? displayH : 1;
  rawMaxX_  = displayW_;
  rawMaxY_  = displayH_;

  resetController();

  if (!probe(kAddr1) && !probe(kAddr2)) {
    Serial1.println("[touch] GT9xx not found.");
    return false;
  }

  readLimits();
  write16(kRegCmd,  0x00);
  write16(kRegStat, 0x00);
  pinMode(intPin_, INPUT_PULLUP);
  Serial1.println("[touch] ready.");
  return true;
}

bool TouchInput::poll(uint16_t* outX, uint16_t* outY)
{
  if (address_ == 0) return false;

  const unsigned long now = millis();
  if (now - lastPollMs_ < kPollMs) return false;
  lastPollMs_ = now;

  uint8_t status = 0;
  if (!read16(kRegStat, &status, 1)) return false;

  const int intLevel = digitalRead(intPin_);
  const bool shouldRead = gt911StatusRequestsRead(status, intLevel);

  if (!shouldRead) {
    // No touch reported. Clear pressed_ so the next finger-down counts as
    // a fresh edge event.
    pressed_ = false;
    return false;
  }

  // Touch is being reported. Read coordinates, then clear the status flag
  // so the controller can latch the next sample.
  uint8_t raw[8] = {};
  const bool ok = read16(kRegPoint1, raw, sizeof(raw));
  write16(kRegStat, 0x00);
  if (!ok) return false;

  const uint8_t pointCount = status & 0x0F;
  const bool allZero = raw[1] == 0 && raw[2] == 0 && raw[3] == 0 && raw[4] == 0;
  if (pointCount == 0 && allZero) {
    pressed_ = false;
    return false;
  }

  if (pressed_) {
    // Finger is still down from a previous edge -- swallow.
    return false;
  }
  pressed_ = true;

  const uint16_t rawX = static_cast<uint16_t>(raw[1] | (raw[2] << 8));
  const uint16_t rawY = static_cast<uint16_t>(raw[3] | (raw[4] << 8));

  TouchDisplayPoint mapped = {};
  if (!mapTouchToDisplay(rawX, rawY, rawMaxX_, rawMaxY_,
                         displayW_, displayH_, &mapped)) {
    return false;
  }

  if (outX) *outX = mapped.x;
  if (outY) *outY = mapped.y;
  Serial1.printf("[touch] tap raw=(%u,%u) screen=(%u,%u)\n",
                 rawX, rawY, mapped.x, mapped.y);
  return true;
}
