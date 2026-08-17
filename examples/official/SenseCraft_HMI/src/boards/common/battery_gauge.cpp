#include "boards/common/battery_gauge.h"

#include <limits.h>

#include "ArduinoLog.h"

namespace {
constexpr float NO_BATTERY_DETECTED_VALUE = -1.0f;
constexpr int MIN_VALID_VOLTAGE_MV = 500;
}

BatteryGauge::BatteryGauge(const Config& config)
    : config_(config)
{
}

void BatteryGauge::init()
{
    if (config_.power_en_pin >= 0) {
        pinMode(config_.power_en_pin, OUTPUT);
        digitalWrite(config_.power_en_pin, HIGH);
    }
    if (config_.adc_en_pin >= 0) {
        pinMode(config_.adc_en_pin, OUTPUT);
        digitalWrite(config_.adc_en_pin, HIGH);
    }

    analogReadResolution(12);
    analogSetPinAttenuation(config_.adc_pin, ADC_11db);
}

bool BatteryGauge::isInserted(bool pmic_connected)
{
    switch (config_.presence_mode) {
    case PresenceMode::Pmic:
        return pmic_connected;
    case PresenceMode::Ripple50:
        return detectByRipple(50, 10, 200, "EE02/EE03");
    case PresenceMode::Ripple10:
        return detectByRipple(10, 25, 300, "EE04");
    }

    return pmic_connected;
}

bool BatteryGauge::isConnected(bool pmic_connected)
{
    bool inserted = isInserted(pmic_connected);
    if (inserted) {
        Log.verboseln("[Board] Battery detected.");
    } else {
        Log.verboseln("[Board] No battery detected.");
    }
    return inserted;
}

float BatteryGauge::readVoltage(bool pmic_connected)
{
    if (!isInserted(pmic_connected)) {
        return NO_BATTERY_DETECTED_VALUE;
    }

    int avg_mv = 0;
    if (!sampleAverageMv(avg_mv)) {
        return NO_BATTERY_DETECTED_VALUE;
    }

    float v_adc = avg_mv / 1000.0f;
    return v_adc * 2.0f;
}

float BatteryGauge::readPercent(bool pmic_connected)
{
    float voltage = readVoltage(pmic_connected);
    if (voltage == NO_BATTERY_DETECTED_VALUE) {
        return NO_BATTERY_DETECTED_VALUE;
    }

    float percent = voltageToPercent(voltage);
    if (percent < 0.0f) {
        return 0.0f;
    }
    if (percent > 100.0f) {
        return 100.0f;
    }
    return percent;
}

bool BatteryGauge::collectSamples(int sample_count, int interval_ms, SampleStats& stats)
{
    if (config_.power_en_pin >= 0) {
        digitalWrite(config_.power_en_pin, HIGH);
    }
    if (config_.adc_en_pin >= 0) {
        digitalWrite(config_.adc_en_pin, HIGH);
    }
    delay(5);

    stats.sum_mv = 0;
    stats.min_mv = INT_MAX;
    stats.max_mv = 0;
    stats.count = 0;

    for (int i = 0; i < sample_count; ++i) {
        int current_mv = analogReadMilliVolts(config_.adc_pin);
        stats.sum_mv += current_mv;
        if (current_mv < stats.min_mv) {
            stats.min_mv = current_mv;
        }
        if (current_mv > stats.max_mv) {
            stats.max_mv = current_mv;
        }
        stats.count++;
        if (i + 1 < sample_count) {
            delay(interval_ms);
        }
    }

    if (config_.adc_en_pin >= 0) {
        digitalWrite(config_.adc_en_pin, LOW);
    }

    return stats.count == sample_count;
}

bool BatteryGauge::detectByRipple(int sample_count, int interval_ms, int ripple_threshold_mv, const char* label)
{
    SampleStats stats;
    if (!collectSamples(sample_count, interval_ms, stats)) {
        Log.errorln("[Board][BatteryDetect:%s] Failed to collect ADC samples", label);
        return false;
    }

    int diff_mv = stats.max_mv - stats.min_mv;
    Log.verboseln("[Board][BatteryDetect:%s] Min: %d mV, Max: %d mV, Diff: %d mV",
                  label, stats.min_mv, stats.max_mv, diff_mv);

    if (stats.max_mv < MIN_VALID_VOLTAGE_MV) {
        Log.verboseln("[Board][BatteryDetect:%s] Voltage too low (%d mV)", label, stats.max_mv);
        return false;
    }

    if (diff_mv > ripple_threshold_mv) {
        Log.verboseln("[Board][BatteryDetect:%s] Ripple %d mV > %d mV -> no battery",
                      label, diff_mv, ripple_threshold_mv);
        return false;
    }

    return true;
}

bool BatteryGauge::sampleAverageMv(int& avg_mv)
{
    constexpr int NUM_SAMPLES = 10;
    constexpr int DELAY_BETWEEN_SAMPLES_MS = 25;

    SampleStats stats;
    if (!collectSamples(NUM_SAMPLES, DELAY_BETWEEN_SAMPLES_MS, stats)) {
        Log.errorln("[Board] Failed to collect ADC samples for averaging");
        return false;
    }

    avg_mv = stats.sum_mv / NUM_SAMPLES;
    int range_mv = stats.max_mv - stats.min_mv;

    Log.verboseln("[ADC Avg] Avg: %d mV, Min: %d mV, Max: %d mV, Range: %d mV\n",
                  avg_mv, stats.min_mv, stats.max_mv, range_mv);
    return true;
}

float BatteryGauge::voltageToPercent(float voltage) const
{
    if (config_.curve_size == 0) {
        return 0.0f;
    }

    if (voltage <= config_.curve[0].voltage) {
        return config_.curve[0].soc;
    }
    if (voltage >= config_.curve[config_.curve_size - 1].voltage) {
        return config_.curve[config_.curve_size - 1].soc;
    }

    for (size_t i = 0; i < config_.curve_size - 1; ++i) {
        const auto& low = config_.curve[i];
        const auto& high = config_.curve[i + 1];
        if (voltage >= low.voltage && voltage <= high.voltage) {
            if (high.voltage == low.voltage) {
                return high.soc;
            }
            float ratio = (voltage - low.voltage) / (high.voltage - low.voltage);
            return low.soc + ratio * (high.soc - low.soc);
        }
    }

    return config_.curve[config_.curve_size - 1].soc;
}
