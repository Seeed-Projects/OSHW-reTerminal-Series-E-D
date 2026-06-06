#pragma once

// Convert the measured ADC millivolts (the DIVIDED battery voltage) into a
// 0-100 percentage. Hardware halves the real voltage, so multiply by 2.
// Maps 3.3 V -> 0 %, 4.2 V -> 100 %, clamped.
inline int vmBatteryPercent(int milliVolts)
{
    const float v = (milliVolts / 1000.0f) * 2.0f;
    float pct = (v - 3.3f) / (4.2f - 3.3f) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return static_cast<int>(pct + 0.5f);
}
