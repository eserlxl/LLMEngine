// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "llmengine/providers/api_client.hpp"
#include "llmengine/diagnostics/debug_artifact_manager.hpp"
#include "llmengine/diagnostics/i_artifact_sink.hpp"

namespace LLMEngine {

std::unique_ptr<DebugArtifactManager> DefaultArtifactSink::create(
    const std::string& request_tmp_dir,
    const std::string& base_tmp_dir,
    int log_retention_hours,
    Logger* logger) const {
    return std::make_unique<DebugArtifactManager>(
        request_tmp_dir, base_tmp_dir, log_retention_hours, logger);
}

void DefaultArtifactSink::writeApiResponse(DebugArtifactManager* mgr,
                                           const ::LLMEngineAPI::APIResponse& response,
                                           bool is_error) const {
    if (!mgr)
        return;
    mgr->writeApiResponse(response, is_error);
}

} // namespace LLMEngine
