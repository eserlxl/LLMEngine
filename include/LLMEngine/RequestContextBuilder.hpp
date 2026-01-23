#pragma once
#include "LLMEngine/DebugArtifactManager.hpp"
#include "LLMEngine/IModelContext.hpp"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace LLMEngine {

struct Logger;

struct RequestContext {
    std::string requestTmpDir;
    std::string fullPrompt;
    nlohmann::json finalParams;
    std::unique_ptr<DebugArtifactManager> debugManager;
    bool writeDebugFiles{false};
    std::string analysisType; // preserved for routing/telemetry
};

/**
 * @brief Builder for creating request contexts with thread-safe unique directory generation.
 *
 * Thread-safety guarantees:
 * - The build() method is thread-safe and can be called concurrently from multiple threads.
 * - Uses atomic counter and thread-local RNG for generating unique request directory names.
 * - Each thread has its own thread-local random number generator instance.
 * - The global request counter uses atomic operations with relaxed memory ordering.
 *
 * The implementation ensures uniqueness of request directories even when multiple
 * requests occur in the same millisecond on the same thread by combining:
 * - Timestamp (milliseconds)
 * - Thread ID hash
 * - Atomic counter
 * - Thread-local random component
 */
class RequestContextBuilder {
  public:
    /**
     * @brief Build a request context with thread-safe unique directory generation.
     *
     * This method is thread-safe and can be called concurrently from multiple threads.
     *
     * @param context The model context providing configuration and builders
     * @param prompt The user prompt
     * @param input Input JSON with parameters
     * @param analysis_type Type of analysis to perform
     * @param mode Mode parameter for parameter merging
     * @param prepend_terse_instruction Whether to prepend terse instruction
     * @return RequestContext with unique request directory and merged parameters
     */
    static RequestContext build(IModelContext& context,
                                std::string_view prompt,
                                const nlohmann::json& input,
                                std::string_view analysis_type,
                                std::string_view mode,
                                bool prepend_terse_instruction);
};

} // namespace LLMEngine
