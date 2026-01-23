// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <algorithm>
#include <cstdint>
#include <random>

namespace LLMEngine {

struct BackoffConfig {
    int baseDelayMs; // base delay in ms
    int maxDelayMs;  // cap per attempt in ms
};

constexpr inline uint64_t computeBackoffCapMs(const BackoffConfig& cfg, int attempt) {
    // attempt is 1-based
    const uint64_t factor = 1ULL << (attempt - 1);
    const uint64_t cap = static_cast<uint64_t>(cfg.baseDelayMs) * factor;
    return static_cast<uint64_t>(std::min<uint64_t>(cap, static_cast<uint64_t>(cfg.maxDelayMs)));
}

inline int jitterDelayMs(std::mt19937_64& rng, uint64_t capMs) {
    if (capMs == 0)
        return 0;
    std::uniform_int_distribution<uint64_t> dist(0, capMs);
    return static_cast<int>(dist(rng));
}

} // namespace LLMEngine
