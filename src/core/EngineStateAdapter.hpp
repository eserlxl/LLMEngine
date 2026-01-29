// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once

#include "LLMEngine/core/IModelContext.hpp"
#include "EngineState.hpp"

#include <memory>
#include <shared_mutex>
#include <string>

namespace LLMEngine {

class EngineStateAdapter : public IModelContext {
    std::shared_ptr<LLMEngine::EngineState> s;

  public:
    explicit EngineStateAdapter(std::shared_ptr<LLMEngine::EngineState> st) : s(std::move(st)) {}
    std::string getTempDirectory() const override {
        std::shared_lock lock(s->configMutex_);
        return s->tmp_dir_;
    }
    std::shared_ptr<IPromptBuilder> getTersePromptBuilder() const override {
        std::shared_lock lock(s->configMutex_);
        return s->tersePromptBuilder_;
    }
    std::shared_ptr<IPromptBuilder> getPassthroughPromptBuilder() const override {
        std::shared_lock lock(s->configMutex_);
        return s->passthroughPromptBuilder_;
    }
    const nlohmann::json& getModelParams() const override {
        std::shared_lock lock(s->configMutex_);
        return s->modelParams_;
    }
    bool areDebugFilesEnabled() const override {
        std::shared_lock lock(s->configMutex_);
        if (!s->debug_)
            return false;
        if (s->debugFilesPolicy_)
            return s->debugFilesPolicy_();
        return !s->disableDebugFilesEnvCached_;
    }
    std::shared_ptr<IArtifactSink> getArtifactSink() const override {
        std::shared_lock lock(s->configMutex_);
        return s->artifactSink_;
    }
    int getLogRetentionHours() const override {
        std::shared_lock lock(s->configMutex_);
        return s->logRetentionHours_;
    }
    std::shared_ptr<Logger> getLogger() const override {
        std::shared_lock lock(s->configMutex_);
        return s->logger_;
    }
    void prepareTempDirectory() override {
        s->ensureSecureTmpDir();
    }
};

} // namespace LLMEngine
