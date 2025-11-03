// Copyright © 2025
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../src/Backoff.hpp"
#include <cassert>
#include <vector>
#include <iostream>

int main() {
    BackoffConfig cfg{100, 10'000}; // base 100ms, max 10s
    std::mt19937_64 rng(123456789ULL);
    std::vector<int> delays;
    for (int attempt = 1; attempt <= 5; ++attempt) {
        const auto cap = computeBackoffCapMs(cfg, attempt);
        delays.push_back(jitterDelayMs(rng, cap));
    }
    // Deterministic expectations for seed 123456789 with uniform [0, cap]
    // Check basic invariants without pinning exact values across platforms:
    //  - All delays are within [0, cap]
    //  - Caps grow exponentially (100,200,400,800,1600)
    std::vector<uint64_t> caps;
    for (int attempt = 1; attempt <= 5; ++attempt) caps.push_back(computeBackoffCapMs(cfg, attempt));
    assert(caps[0] == 100 && caps[1] == 200 && caps[2] == 400 && caps[3] == 800 && caps[4] == 1600);
    for (size_t i = 0; i < delays.size(); ++i) {
        assert(delays[i] >= 0);
        assert(static_cast<uint64_t>(delays[i]) <= caps[i]);
    }
    std::cout << "test_backoff: OK\n";
    return 0;
}


