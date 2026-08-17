# Runtime Scheduler Overview

This firmware uses a lightweight cooperative scheduler to run multiple app
tasks with predictable timing budgets. The core is split across focused header
files under `include/rt/core/` so you can read or reuse individual pieces on
their own.

## Module Layout

- `config.h` – compile-time limits for the high/low priority lanes.
- `status.h` – shared status codes returned by the framework callbacks.
- `time.h` – clock abstraction and helpers (`Milliseconds`, `Microseconds`).
- `context.h` – frame timing snapshot passed into every app callback.
- `app.h` – app lifecycle interface (`App`, `AppState`, `Budget`, `AppSlot`).
- `lanes.h` – round-robin queues that service high/low priority apps.
- `scheduler.h` – orchestrates both lanes, applies per-frame/ per-app budgets.
- `system.h` – convenience facade bundling a clock with the scheduler.
- `rt_core.h` – umbrella header that re-exports everything above.

When adding features, prefer editing the dedicated module so the umbrella file
remains an easy entry point for contributors.

## Arduino Quick Start Example

Below is an Arduino sketch that blinks the built-in LED in a simple on/off
cycle while running inside the cooperative scheduler. The LED toggles every
250&nbsp;ms, so you will see two quick flashes per second followed by two short
off periods.

```cpp
#include <Arduino.h>
#include "rt_core.h"

constexpr int kLedPin = LED_BUILTIN;

static rt::Milliseconds system_now_ms() {
    return millis();
}

class BlinkerApp final : public rt::App {
public:
    const char *name() const override { return "Blinker"; }

    rt::Status onCreate(rt::AppCtx &) override {
        pinMode(kLedPin, OUTPUT);
        digitalWrite(kLedPin, LOW);
        return rt::Status::Ok;
    }

    rt::Status onStep(rt::AppCtx &ctx, const rt::Budget &) override {
        elapsed_ += ctx.delta_ms;
        if (elapsed_ >= 250) {
            elapsed_ = 0;
            level_ = !level_;
            digitalWrite(kLedPin, level_ ? HIGH : LOW);
        }
        return rt::Status::Ok;
    }

private:
    bool level_{false};
    rt::Milliseconds elapsed_{0};
};

static rt::Clock clock{system_now_ms};
static rt::AppSystem app_system(&clock);
static BlinkerApp blinker;

void setup() {
    app_system.scheduler().install(/*app id=*/1, &blinker, /*high_lane=*/true);
    app_system.scheduler().start(1, true);
}

void loop() {
    app_system.run_once();
}

```

The scheduler keeps track of frame timing via the `millis()` clock callback and
calls `onStep` for each running app while enforcing microsecond budgets. Add
more apps by installing them on either the high-priority or low-priority lane
depending on their latency needs.
