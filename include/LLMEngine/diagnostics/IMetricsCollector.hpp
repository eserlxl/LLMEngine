// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace LLMEngine {

/**
 * @brief Tag for metric dimensions.
 */
struct MetricTag {
    std::string name;
    std::string value;
};

/**
 * @brief Interface for collecting engine metrics.
 *
 * Implement this interface to hook LLMEngine into your application's
 * monitoring system (e.g., Prometheus, StatsD).
 */
class LLMENGINE_EXPORT IMetricsCollector {
  public:
    virtual ~IMetricsCollector() = default;

    /**
     * @brief Record the duration of an operation.
     * @param operation Operation name (e.g., "analyze").
     * @param milliseconds Duration in ms.
     * @param tags Contextual tags (provider, model, etc.).
     */
    virtual void recordLatency(std::string_view operation,
                               long milliseconds,
                               const std::vector<MetricTag>& tags) = 0;

    /**
     * @brief Increment a counter.
     * @param name Counter name (e.g., "tokens_total").
     * @param value Amount to increment.
     * @param tags Contextual tags.
     */
    virtual void recordCounter(std::string_view name,
                               long value,
                               const std::vector<MetricTag>& tags) = 0;
};

} // namespace LLMEngine
