#ifndef E1003_TOUCH_MAPPER_H
#define E1003_TOUCH_MAPPER_H

#include <stdint.h>

struct TouchDisplayPoint {
  uint16_t x;
  uint16_t y;
};

static inline bool mapTouchToDisplay(uint16_t rawX, uint16_t rawY,
                                     uint16_t rawMaxX, uint16_t rawMaxY,
                                     uint16_t displayW, uint16_t displayH,
                                     TouchDisplayPoint* out)
{
  if (out == 0 || rawMaxX == 0 || rawMaxY == 0 || displayW == 0 || displayH == 0) {
    return false;
  }

  const uint32_t scaledX = (static_cast<uint32_t>(rawX) * displayW) / rawMaxX;
  const uint32_t scaledY = (static_cast<uint32_t>(rawY) * displayH) / rawMaxY;

  out->x = static_cast<uint16_t>((scaledX >= displayW) ? (displayW - 1) : scaledX);
  out->y = static_cast<uint16_t>((scaledY >= displayH) ? (displayH - 1) : scaledY);
  return true;
}

static inline bool resolveDisplaySize(uint16_t measuredW, uint16_t measuredH,
                                      uint16_t fallbackW, uint16_t fallbackH,
                                      TouchDisplayPoint* out)
{
  if (out == 0) return false;

  out->x = measuredW ? measuredW : fallbackW;
  out->y = measuredH ? measuredH : fallbackH;
  return out->x != 0 && out->y != 0;
}

static inline bool gt911StatusRequestsRead(uint8_t status, int intLevel)
{
  const bool dataReady = (status & 0x80U) != 0;
  const bool hasPointCount = (status & 0x0FU) != 0;
  const bool intActiveLow = intLevel == 0;
  return dataReady || hasPointCount || intActiveLow;
}

#endif
