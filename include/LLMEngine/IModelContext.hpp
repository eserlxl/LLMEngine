// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace LLMEngine {
class IPromptBuilder;
struct Logger;
} // namespace LLMEngine

// Forward declaration in namespace
namespace LLMEngine {
class IArtifactSink;
}

namespace LLMEngine {

/**
 * @brief Minimal interface for model context needed by request builders.
 *
 * This interface breaks cyclic dependencies by allowing RequestContextBuilder
 * and other helpers to access engine state without including the full LLMEngine
 * header. This improves compile times and reduces coupling.
 *
 * ## Thread Safety
 *
 * Implementations should follow the same thread-safety guarantees as LLMEngine
 * (typically not thread-safe for modifications, but safe for read-only access).
 */
class LLMENGINE_EXPORT IModelContext {
  public:
    virtual ~IModelContext() = default;

    /**
     * @brief Get the temporary directory path.
     */
    [[nodiscard]] virtual std::string getTempDirectory() const = 0;

    /**
     * @brief Get the terse prompt builder.
     */
    [[nodiscard]] virtual std::shared_ptr<IPromptBuilder> getTersePromptBuilder() const = 0;

    /**
     * @brief Get the passthrough prompt builder.
     */
    [[nodiscard]] virtual std::shared_ptr<IPromptBuilder> getPassthroughPromptBuilder() const = 0;

    /**
     * @brief Get the model parameters.
     */
    [[nodiscard]] virtual const nlohmann::json& getModelParams() const = 0;

    /**
     * @brief Check if debug files are enabled.
     */
    [[nodiscard]] virtual bool areDebugFilesEnabled() const = 0;

    /**
     * @brief Get the artifact sink.
     */
    [[nodiscard]] virtual std::shared_ptr<IArtifactSink> getArtifactSink() const = 0;

    /**
     * @brief Get log retention hours.
     */
    [[nodiscard]] virtual int getLogRetentionHours() const = 0;

    /**
     * @brief Get the logger.
     */
    [[nodiscard]] virtual std::shared_ptr<Logger> getLogger() const = 0;

    /**
     * @brief Prepare the temporary directory (ensure it exists and is secure).
     */
    virtual void prepareTempDirectory() const = 0;
};

} // namespace LLMEngine
