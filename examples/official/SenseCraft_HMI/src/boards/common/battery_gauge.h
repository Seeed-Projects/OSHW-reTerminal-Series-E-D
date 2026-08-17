#pragma once

#include <Arduino.h>
#include <limits.h>
#include <stddef.h>

struct BatteryCurvePoint {
    float soc;
    float voltage;
};

class BatteryGauge {
public:
    enum class PresenceMode {
        Pmic,
        Ripple10,
        Ripple50,
    };

    struct Config {
        int adc_pin;
        int power_en_pin;
        int adc_en_pin;
        const BatteryCurvePoint* curve;
        size_t curve_size;
        PresenceMode presence_mode;
    };

    explicit BatteryGauge(const Config& config);

    void init();
    bool isInserted(bool pmic_connected);
    bool isConnected(bool pmic_connected);
    float readVoltage(bool pmic_connected);
    float readPercent(bool pmic_connected);

private:
    struct SampleStats {
        long sum_mv = 0;
        int min_mv = INT_MAX;
        int max_mv = 0;
        int count = 0;
    };

    bool collectSamples(int sample_count, int interval_ms, SampleStats& stats);
    bool detectByRipple(int sample_count, int interval_ms, int ripple_threshold_mv, const char* label);
    bool sampleAverageMv(int& avg_mv);
    float voltageToPercent(float voltage) const;

    Config config_;
};
