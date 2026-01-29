
// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/core/LLMEngineBuilder.hpp"
#include "LLMEngine/providers/APIClient.hpp"
#include "LLMEngine/providers/ProviderBootstrap.hpp"

namespace LLMEngine {

LLMEngineBuilder& LLMEngineBuilder::withProvider(std::string_view name) {
    provider_name_ = std::string(name);
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withApiKey(std::string_view key) {
    api_key_ = std::string(key);
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withModel(std::string_view model) {
    model_ = std::string(model);
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withModelParams(const nlohmann::json& params) {
    model_params_ = params;
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withConfigManager(std::shared_ptr<::LLMEngineAPI::IConfigManager> cfg) {
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

LLMEngineBuilder& LLMEngineBuilder::withBaseUrl(std::string_view url) {
    base_url_ = std::string(url);
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withTimeout(std::chrono::milliseconds timeout) {
    timeout_ms_ = static_cast<int>(timeout.count());
    return *this;
}

LLMEngineBuilder& LLMEngineBuilder::withMaxRetries(int retries) {
    max_retries_ = retries;
    return *this;
}

std::unique_ptr<LLMEngine> LLMEngineBuilder::build() {
    // If provider_name_ is not set but we have a config manager, maybe we can infer?
    // But currently LLMEngine needs at least a provider name or type.
    if (provider_name_.empty()) {
        throw std::runtime_error("Provider name must be set in LLMEngineBuilder");
    }

    // Construct using the comprehensive constructor
    auto engine = std::make_unique<LLMEngine>(
        provider_name_,
        api_key_,
        model_,
        model_params_,
        log_retention_hours_,
        debug_,
        config_manager_,
        base_url_
    );

    if (logger_) {
        engine->setLogger(logger_);
    }

    // Configure Default Request Options
    RequestOptions defaults;
    if (timeout_ms_) defaults.timeout_ms = *timeout_ms_;
    if (max_retries_) defaults.max_retries = *max_retries_;
    
    // If we have defaults, set them
    if (timeout_ms_ || max_retries_) {
        engine->setDefaultRequestOptions(defaults);
    }

    return engine;
}

} // namespace LLMEngine
