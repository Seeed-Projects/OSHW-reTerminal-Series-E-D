#include "GT911.h"

GT911::GT911() {}

void GT911::begin(int intPin, int rstPin, uint16_t width, uint16_t height, TwoWire &wire) {
    _intPin = intPin;
    _rstPin = rstPin;
    _width = width;
    _height = height;
    _wire = &wire;

    pinMode(_intPin, OUTPUT);
    pinMode(_rstPin, OUTPUT);

    reset();
    readResolution();
}

void GT911::reset() {
    digitalWrite(_rstPin, LOW);
    digitalWrite(_intPin, LOW);
    delay(12); 

    digitalWrite(_rstPin, HIGH);
    delay(8); 

    pinMode(_intPin, INPUT);
    _addr = GT911_ADDR1; 
    delay(50);
}

void GT911::readResolution() {
    uint8_t buf[4];
    readRegisters(GT911_REG_COORD_RES, buf, 4);
    _maxX_Sensor = (buf[1] << 8) | buf[0];
    _maxY_Sensor = (buf[3] << 8) | buf[2];
    if (_maxX_Sensor == 0) _maxX_Sensor = 2048;
    if (_maxY_Sensor == 0) _maxY_Sensor = 2048;
    Serial1.printf("GT911 Resolution: %d x %d\n", _maxX_Sensor, _maxY_Sensor);
}

void GT911::onTouch(TouchCallback callback) {
    _callback = callback;
}

void GT911::onGesture(GestureCallback callback) {
    _gestureCallback = callback;
}

void GT911::setGestureConfig(const GTGestureConfig &config) {
    _gestureConfig = config;
}

void GT911::setRotation(uint8_t rotation) {
    _rotation = rotation;
}

void GT911::enterGestureMode() {
    writeRegister(GT911_REG_COMMAND2, GT911_CMD_GESTURE_MODE);
    delay(1);
    writeRegister(GT911_REG_COMMAND, GT911_CMD_GESTURE_MODE);
    delay(10);
}

void GT911::clearGesture() {
    writeRegister(GT911_REG_GESTURE_TYPE, 0x00);
}

static inline uint32_t gt_abs32(int32_t v) {
    return v < 0 ? static_cast<uint32_t>(-v) : static_cast<uint32_t>(v);
}

void GT911::handleGestureSample(uint8_t touchCount, const GTPoint *points, uint32_t nowMs) {
    if (touchCount == 0 || points == nullptr) {
        return;
    }

    if (touchCount != 1) {
        if (!_touchActive) {
            _touchStartMs = nowMs;
            _touchStartPoint = points[0];
            _lastPoint = points[0];
            _touchActive = true;
        } else {
            _lastPoint = points[0];
        }
        _gestureIgnore = true;
        return;
    }

    if (!_touchActive) {
        _touchStartMs = nowMs;
        _touchStartPoint = points[0];
        _lastPoint = points[0];
        _touchActive = true;
        _gestureIgnore = false;
        return;
    }

    _lastPoint = points[0];
}

void GT911::handleGestureRelease(uint32_t nowMs) {
    if (!_touchActive) {
        return;
    }

    if (_gestureIgnore) {
        _touchActive = false;
        _gestureIgnore = false;
        return;
    }

    const int32_t dx = static_cast<int32_t>(_lastPoint.x) - static_cast<int32_t>(_touchStartPoint.x);
    const int32_t dy = static_cast<int32_t>(_lastPoint.y) - static_cast<int32_t>(_touchStartPoint.y);
    const uint32_t absDx = gt_abs32(dx);
    const uint32_t absDy = gt_abs32(dy);
    const uint32_t duration = nowMs - _touchStartMs;

    if (_lastTapMs > 0 && (nowMs - _lastTapMs) > _gestureConfig.double_tap_max_interval_ms) {
        _lastTapMs = 0;
    }

    const bool isSwipe = duration <= _gestureConfig.swipe_max_duration_ms &&
                         (absDx >= _gestureConfig.swipe_min_distance || absDy >= _gestureConfig.swipe_min_distance);

    if (isSwipe) {
        if (_gestureCallback) {
            GTGestureEvent event = {};
            event.start = _touchStartPoint;
            event.end = _lastPoint;
            event.duration_ms = duration;
            if (absDx >= absDy) {
                event.type = (dx >= 0) ? GTGesture::SwipeRight : GTGesture::SwipeLeft;
            } else {
                event.type = (dy >= 0) ? GTGesture::SwipeDown : GTGesture::SwipeUp;
            }
            _gestureCallback(event);
        }
        _lastTapMs = 0;
    } else {
        const uint32_t maxMove = (absDx > absDy) ? absDx : absDy;
        const bool isTap = duration <= _gestureConfig.tap_max_duration_ms &&
                           maxMove <= _gestureConfig.tap_max_movement;

        if (isTap) {
            bool isDoubleTap = false;
            if (_lastTapMs > 0 && (nowMs - _lastTapMs) <= _gestureConfig.double_tap_max_interval_ms) {
                const int32_t tapDx = static_cast<int32_t>(_touchStartPoint.x) - static_cast<int32_t>(_lastTapPoint.x);
                const int32_t tapDy = static_cast<int32_t>(_touchStartPoint.y) - static_cast<int32_t>(_lastTapPoint.y);
                const uint32_t tapDistSq = gt_abs32(tapDx) * gt_abs32(tapDx) + gt_abs32(tapDy) * gt_abs32(tapDy);
                const uint32_t maxDist = _gestureConfig.double_tap_max_distance;
                if (tapDistSq <= static_cast<uint32_t>(maxDist) * static_cast<uint32_t>(maxDist)) {
                    isDoubleTap = true;
                }
            }

            if (isDoubleTap) {
                if (_gestureCallback) {
                    GTGestureEvent event = {};
                    event.type = GTGesture::DoubleTap;
                    event.start = _touchStartPoint;
                    event.end = _touchStartPoint;
                    event.duration_ms = duration;
                    _gestureCallback(event);
                }
                _lastTapMs = 0;
            } else {
                _lastTapMs = nowMs;
                _lastTapPoint = _touchStartPoint;
            }
        } else {
            _lastTapMs = 0;
        }
    }

    _touchActive = false;
    _gestureIgnore = false;
}

void GT911::loop() {
    uint8_t status = readRegister(GT911_REG_STATUS);

    if ((status & 0x80) == 0) {
        return; 
    }

    uint8_t touchCount = status & 0x0F;
    uint32_t nowMs = millis();

    if (touchCount > 0 && touchCount <= 5) {
        GTPoint points[5];
        uint8_t rawData[40];

        readRegisters(GT911_REG_POINTS, rawData, touchCount * 8);

        for (int i = 0; i < touchCount; i++) {
            uint8_t *ptData = &rawData[i * 8];

            uint16_t x = ptData[0] | (ptData[1] << 8);
            uint16_t y = ptData[2] | (ptData[3] << 8);
            uint16_t size = ptData[4] | (ptData[5] << 8);
            uint8_t id = ptData[7];

            uint16_t mapX, mapY;
            mapX = map(x, 0, _maxX_Sensor, 0, _width);
            mapY = map(y, 0, _maxY_Sensor, 0, _height);

            switch (_rotation) {
                case 1:
                    points[i].x = mapY;
                    points[i].y = _width - mapX;
                    break;
                case 2:
                    points[i].x = _width - mapX;
                    points[i].y = _height - mapY;
                    break;
                case 3:
                    points[i].x = _height - mapY;
                    points[i].y = mapX;
                    break;
                default:
                    points[i].x = mapX;
                    points[i].y = mapY;
                    break;
            }
            
            points[i].id = id;
            points[i].size = size;
        }

        handleGestureSample(touchCount, points, nowMs);

        if (_callback) {
            _callback(touchCount, points);
        }
    } else if (touchCount == 0) {
        handleGestureRelease(nowMs);
    } else {
        _gestureIgnore = true;
    }

    writeRegister(GT911_REG_STATUS, 0);
}

void GT911::writeRegister(uint16_t reg, uint8_t val) {
    _wire->beginTransmission(_addr);
    _wire->write(reg >> 8);
    _wire->write(reg & 0xFF);
    _wire->write(val);
    _wire->endTransmission();
}

uint8_t GT911::readRegister(uint16_t reg) {
    uint8_t val = 0;
    readRegisters(reg, &val, 1);
    return val;
}

void GT911::readRegisters(uint16_t reg, uint8_t *buffer, uint8_t len) {
    _wire->beginTransmission(_addr);
    _wire->write(reg >> 8);
    _wire->write(reg & 0xFF);
    _wire->endTransmission();

    _wire->requestFrom(_addr, len);
    for (int i = 0; i < len; i++) {
        if (_wire->available()) {
            buffer[i] = _wire->read();
        }
    }
}
