// Pure weather "vibe" calculation. Inputs in Fahrenheit + millimeters of rain.
// Outputs are 0-10 levels for wet / temperature / "fuk yea" score.

#ifndef PURE_WEATHER_LOGIC_H
#define PURE_WEATHER_LOGIC_H

namespace pure {

inline int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

inline void calcWeatherLevels(float tempF, float rainMm, int& wet, int& tmp, int& fuk) {
    float rainInches = rainMm / 25.4f;
    wet = clamp((int)(rainInches * 5.0f), 0, 10);

    if (tempF <= 32.0f)        tmp = 0;
    else if (tempF >= 100.0f)  tmp = 10;
    else                       tmp = clamp((int)((tempF - 32.0f) / 6.8f), 0, 10);

    float diff = tempF - 65.0f;
    if (diff < 0) diff = -diff;
    int tempScore = (tempF < 32.0f || tempF > 100.0f)
                    ? 0
                    : clamp(10 - (int)(diff / 5.0f), 0, 10);
    int rainPenalty = clamp((int)(rainInches * 5.0f), 0, 10);
    fuk = clamp(tempScore - rainPenalty, 0, 10);
}

}  // namespace pure

#endif  // PURE_WEATHER_LOGIC_H
