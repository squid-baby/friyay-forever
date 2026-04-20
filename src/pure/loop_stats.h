#ifndef PURE_LOOP_STATS_H
#define PURE_LOOP_STATS_H

// Main-loop tick timing aggregator (Phase 3.2.3).
//
// Records per-iteration wall-clock time and counts how often a tick exceeds a
// freeze threshold. Pure — no Arduino deps, directly unit-testable. `main.cpp`
// owns a single instance and feeds it `millis()` deltas every iteration; the
// `/diag` Telegram command reads the aggregates.
//
// Invariants:
// - `sumMs` is 64-bit so a multi-hour average doesn't wrap.
// - `slowCount` bumps only when a tick is STRICTLY greater than `thresholdMs`,
//   so `threshold=100` counts ticks of 101 ms and up (matches PLAN.md wording
//   "exceeds 100 ms").

#include <stdint.h>

namespace pure {

struct LoopStats {
    uint32_t thresholdMs;   // tick time above this counts as "slow"
    uint32_t sampleCount;   // total ticks recorded
    uint32_t slowCount;     // ticks strictly greater than thresholdMs
    uint32_t maxMs;         // worst tick observed
    uint64_t sumMs;         // sum of all tick times, for mean
};

inline void initLoopStats(LoopStats& s, uint32_t thresholdMs) {
    s.thresholdMs = thresholdMs;
    s.sampleCount = 0;
    s.slowCount = 0;
    s.maxMs = 0;
    s.sumMs = 0;
}

inline void recordTick(LoopStats& s, uint32_t tickMs) {
    s.sampleCount++;
    s.sumMs += tickMs;
    if (tickMs > s.maxMs) s.maxMs = tickMs;
    if (tickMs > s.thresholdMs) s.slowCount++;
}

inline uint32_t meanTickMs(const LoopStats& s) {
    if (s.sampleCount == 0) return 0;
    return (uint32_t)(s.sumMs / s.sampleCount);
}

// Returns 0..10000 (basis points). Using bps avoids float in firmware and keeps
// "0.03% slow" meaningful without rounding to 0%.
inline uint32_t slowTickBasisPoints(const LoopStats& s) {
    if (s.sampleCount == 0) return 0;
    return (uint32_t)((uint64_t)s.slowCount * 10000u / s.sampleCount);
}

}  // namespace pure

#endif
