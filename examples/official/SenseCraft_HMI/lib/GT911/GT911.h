#ifndef GT911_H
#define GT911_H

#include <Arduino.h>
#include <Wire.h>
#include <functional>

#define GT911_ADDR1 0x5D
#define GT911_ADDR2 0x14

#define GT911_REG_COMMAND        0x8040
#define GT911_REG_COMMAND2       0x8046
#define GT911_REG_CONFIG_START   0x8047
#define GT911_REG_ID             0x8140
#define GT911_REG_COORD_RES      0x8146 
#define GT911_REG_STATUS         0x814E 
#define GT911_REG_POINTS         0x8150 

#define GT911_REG_GESTURE_TYPE   0x814B
#define GT911_CMD_GESTURE_MODE   0x08
#define GT911_CMD_SLEEP_MODE     0x05

struct GTPoint {
    uint16_t x;
    uint16_t y;
    uint16_t size;
    uint8_t id;
};

typedef std::function<void(int8_t, GTPoint*)> TouchCallback;

enum class GTGesture : uint8_t {
    None = 0,
    SwipeLeft,
    SwipeRight,
    SwipeUp,
    SwipeDown,
    DoubleTap,
};

struct GTGestureEvent {
    GTGesture type;
    GTPoint start;
    GTPoint end;
    uint32_t duration_ms;
};

typedef std::function<void(const GTGestureEvent&)> GestureCallback;

struct GTGestureConfig {
    uint16_t swipe_min_distance = 60;
    uint16_t swipe_max_duration_ms = 600;
    uint16_t tap_max_duration_ms = 200;
    uint16_t tap_max_movement = 20;
    uint16_t double_tap_max_interval_ms = 300;
    uint16_t double_tap_max_distance = 40;
};

class GT911 {
public:
    GT911();

    void begin(int intPin, int rstPin, uint16_t width, uint16_t height, TwoWire &wire = Wire);

    void onTouch(TouchCallback callback);
    void onGesture(GestureCallback callback);
    void setGestureConfig(const GTGestureConfig &config);

    void loop();

    void setRotation(uint8_t rotation);

    void enterGestureMode();
    void clearGesture();

private:
    int _intPin;
    int _rstPin;
    uint16_t _width;
    uint16_t _height;
    uint16_t _maxX_Sensor;
    uint16_t _maxY_Sensor;
    uint8_t _addr;
    uint8_t _rotation = 0;
    
    TwoWire *_wire;
    TouchCallback _callback = nullptr;
    GestureCallback _gestureCallback = nullptr;
    GTGestureConfig _gestureConfig = {};
    bool _touchActive = false;
    bool _gestureIgnore = false;
    uint32_t _touchStartMs = 0;
    GTPoint _touchStartPoint = {};
    GTPoint _lastPoint = {};
    uint32_t _lastTapMs = 0;
    GTPoint _lastTapPoint = {};

    void reset();
    void readResolution();
    void writeRegister(uint16_t reg, uint8_t val);
    uint8_t readRegister(uint16_t reg);
    void readRegisters(uint16_t reg, uint8_t *buffer, uint8_t len);
    void handleGestureSample(uint8_t touchCount, const GTPoint *points, uint32_t nowMs);
    void handleGestureRelease(uint32_t nowMs);
};

#endif
