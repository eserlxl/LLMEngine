// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <memory>
#include <string>
#include <string_view>
#include "LLMEngine/LLMEngineExport.hpp"

namespace LLMEngine { class DebugArtifactManager; class Logger; }
namespace LLMEngineAPI { struct APIResponse; }

namespace LLMEngine {

/**
 * @brief Interface for debug artifact creation and writing.
 */
class LLMENGINE_EXPORT IArtifactSink {
public:
    virtual ~IArtifactSink() = default;

    /**
     * @brief Create a new DebugArtifactManager for a request.
     */
    [[nodiscard]] virtual std::unique_ptr<DebugArtifactManager> create(
        const std::string& request_tmp_dir,
        const std::string& base_tmp_dir,
        int log_retention_hours,
        Logger* logger) const = 0;

    /**
     * @brief Write API response to artifacts.
     */
    virtual void writeApiResponse(
        DebugArtifactManager* mgr,
        const ::LLMEngineAPI::APIResponse& response,
        bool is_error) const = 0;
};

/**
 * @brief Default artifact sink implemented with DebugArtifactManager.
 */
class LLMENGINE_EXPORT DefaultArtifactSink : public IArtifactSink {
public:
    [[nodiscard]] std::unique_ptr<DebugArtifactManager> create(
        const std::string& request_tmp_dir,
        const std::string& base_tmp_dir,
        int log_retention_hours,
        Logger* logger) const override;

    void writeApiResponse(
        DebugArtifactManager* mgr,
        const ::LLMEngineAPI::APIResponse& response,
        bool is_error) const override;
};

} // namespace LLMEngine


