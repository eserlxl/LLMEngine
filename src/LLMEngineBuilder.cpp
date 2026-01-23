// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/LLMEngineBuilder.hpp"
#include "LLMEngine/ProviderBootstrap.hpp"
#include <stdexcept>

namespace LLMEngine {

LLMEngineBuilder& LLMEngineBuilder::withProvider(std::string_view name) {
    provider_name_ = name;
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withApiKey(std::string_view key) {
    api_key_ = key;
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withModel(std::string_view model) {
    model_ = model;
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withModelParams(const nlohmann::json& params) {
    model_params_ = params;
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withConfigManager(
    std::shared_ptr<::LLMEngineAPI::IConfigManager> cfg) {
    config_manager_ = std::move(cfg);
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withLogger(std::shared_ptr<Logger> logger) {
    logger_ = std::move(logger);
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::enableDebug(bool enabled) {
    debug_ = enabled;
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withLogRetention(int hours) {
    log_retention_hours_ = hours;
    return *this;
}

std::unique_ptr<LLMEngine> LLMEngineBuilder::build() {
    if (provider_name_.empty()) {
        // Fallback to default provider from config manager if available,
        // but we need a config manager to check.
        // If config_manager_ is null, LLMEngine constructor handles looking up singleton.
        // But LLMEngine constructor expects a provider name.
        // We will pass empty provider name which ProviderBootstrap handles (resolves default).
    }

    auto engine = std::make_unique<LLMEngine>(provider_name_,
                                              api_key_,
                                              model_,
                                              model_params_,
                                              log_retention_hours_,
                                              debug_,
                                              config_manager_);

    if (logger_) {
        engine->setLogger(logger_);
    }

    return engine;
}

} // namespace LLMEngine
