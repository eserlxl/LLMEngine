// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/LLMEngine.hpp"

#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/AnalysisInput.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/HttpStatus.hpp"
#include "LLMEngine/IConfigManager.hpp"
#include "LLMEngine/IMetricsCollector.hpp"
#include "LLMEngine/ITempDirProvider.hpp"
#include "LLMEngine/ProviderBootstrap.hpp"
#include "LLMEngine/RequestContextBuilder.hpp"
#include "LLMEngine/ResponseHandler.hpp"
#include "LLMEngine/ParameterMerger.hpp"
#include "LLMEngine/SecureString.hpp"
#include "LLMEngine/TempDirectoryService.hpp"
#include "LLMEngine/Utils/Semaphore.hpp"
#include "LLMEngine/Utils/ThreadPool.hpp"

#include <chrono>
#include <mutex>
#include <shared_mutex>

#include <algorithm>
#include <cctype>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

// Namespace aliases to reduce verbosity
namespace LLM = ::LLMEngine;
namespace LLMAPI = ::LLMEngineAPI;

// Helper function to parse LLMENGINE_DISABLE_DEBUG_FILES environment variable
namespace {
bool parseDisableDebugFilesEnv() {
    const char* env_value = std::getenv("LLMENGINE_DISABLE_DEBUG_FILES");
    if (env_value == nullptr) {
        return false; // Variable not set, enable debug files
    }
    if (env_value[0] == '\0') {
        return false;
    }
    std::string value(env_value);
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });

    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return true;
}
} // namespace

namespace LLMEngine {



// Internal state structure implementation
struct LLMEngine::EngineState {
    std::string model_;
    nlohmann::json model_params_;
    int log_retention_hours_;
    bool debug_;
    std::string tmp_dir_;
    std::shared_ptr<ITempDirProvider> temp_dir_provider_;
    bool tmp_dir_verified_ = false;

    std::unique_ptr<LLMAPI::APIClient> api_client_;
    std::shared_ptr<LLMAPI::IConfigManager> config_manager_;

    std::shared_ptr<IPromptBuilder> terse_prompt_builder_{std::make_shared<TersePromptBuilder>()};
    std::shared_ptr<IPromptBuilder> passthrough_prompt_builder_{
        std::make_shared<PassthroughPromptBuilder>()};
    std::shared_ptr<IRequestExecutor> request_executor_{std::make_shared<DefaultRequestExecutor>()};
    std::shared_ptr<IArtifactSink> artifact_sink_{std::make_shared<DefaultArtifactSink>()};

    LLMAPI::ProviderType provider_type_;
    SecureString api_key_{""};
    std::string ollama_url_;
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<IMetricsCollector> metrics_collector_;
    std::function<bool()> debug_files_policy_;
    bool disable_debug_files_env_cached_;
    std::vector<std::shared_ptr<IInterceptor>> interceptors_;
    std::mutex state_mutex_; // Protects ensuring secure directories
    mutable std::shared_mutex config_mutex_; // Protects dynamic configuration (interceptors, settings)

    EngineState(const nlohmann::json& params, int cleanup_hours, bool debug)
        : model_params_(params), log_retention_hours_(cleanup_hours), debug_(debug),
          disable_debug_files_env_cached_(parseDisableDebugFilesEnv()) {
        // Initialize defaults
        logger_ = std::make_shared<DefaultLogger>();
        temp_dir_provider_ = std::make_shared<DefaultTempDirProvider>();
        tmp_dir_ = temp_dir_provider_->getTempDir();
    }

    void initializeAPIClient() {
        if (provider_type_ != LLMAPI::ProviderType::OLLAMA) {
            if (api_key_.empty()) {
                std::string env_var_name = ProviderBootstrap::getApiKeyEnvVarName(provider_type_);
                std::string error_msg =
                    "No API key found for provider "
                    + LLMAPI::APIClientFactory::providerTypeToString(provider_type_) + ". Set the "
                    + env_var_name + " environment variable or provide it in the constructor.";
                if (logger_) {
                    logger_->log(LogLevel::Error, error_msg);
                }
                throw std::runtime_error(error_msg);
            }
        }

        if (provider_type_ == LLMAPI::ProviderType::OLLAMA) {
            api_client_ = LLMAPI::APIClientFactory::createClient(
                provider_type_, "", model_, ollama_url_, config_manager_);
        } else {
            api_client_ = LLMAPI::APIClientFactory::createClient(
                provider_type_, api_key_.view(), model_, "", config_manager_);
        }

        if (!api_client_) {
            std::string provider_name =
                LLMAPI::APIClientFactory::providerTypeToString(provider_type_);
            throw std::runtime_error("Failed to create API client: " + provider_name);
        }
    }

    void ensureSecureTmpDir() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (tmp_dir_verified_) {
            if (TempDirectoryService::isDirectoryValid(tmp_dir_, logger_.get())) {
                return;
            }
            tmp_dir_verified_ = false;
        }

        auto result = TempDirectoryService::ensureSecureDirectory(tmp_dir_, logger_.get());
        if (!result.success) {
            throw std::runtime_error(result.error_message);
        }
        tmp_dir_verified_ = true;
    }
};

// --- Constructors ---

LLMEngine::LLMEngine(std::unique_ptr<LLMAPI::APIClient> client,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug,
                     const std::shared_ptr<ITempDirProvider>& temp_dir_provider) {
    state_ = std::make_shared<EngineState>(model_params, log_retention_hours, debug);

    if (temp_dir_provider) {
        state_->temp_dir_provider_ = temp_dir_provider;
        state_->tmp_dir_ = temp_dir_provider->getTempDir();
    }

    if (!client) {
        throw std::runtime_error("API client must not be null");
    }
    state_->api_client_ = std::move(client);
    state_->provider_type_ = state_->api_client_->getProviderType();
}

LLMEngine::LLMEngine(LLMAPI::ProviderType provider_type,
                     std::string_view api_key,
                     std::string_view model,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug) {
    state_ = std::make_shared<EngineState>(model_params, log_retention_hours, debug);
    state_->provider_type_ = provider_type;

    // Resolve API key using ProviderBootstrap (respects environment variables)
    state_->api_key_ =
        ProviderBootstrap::resolveApiKey(provider_type, api_key, "", state_->logger_.get());

    state_->model_ = std::string(model);
    state_->config_manager_ = std::shared_ptr<LLMAPI::IConfigManager>(
        &LLMAPI::APIConfigManager::getInstance(), [](LLMAPI::IConfigManager*) {});

    state_->initializeAPIClient();
}

LLMEngine::LLMEngine(std::string_view provider_name,
                     std::string_view api_key,
                     std::string_view model,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug,
                     const std::shared_ptr<LLMAPI::IConfigManager>& config_manager) {
    state_ = std::make_shared<EngineState>(model_params, log_retention_hours, debug);

    // Manage config manager
    if (config_manager) {
        state_->config_manager_ = config_manager;
    } else {
        state_->config_manager_ = std::shared_ptr<LLMAPI::IConfigManager>(
            &LLMAPI::APIConfigManager::getInstance(), [](LLMAPI::IConfigManager*) {});
    }

    auto bootstrap_result = ProviderBootstrap::bootstrap(
        provider_name, api_key, model, state_->config_manager_, state_->logger_.get());

    state_->provider_type_ = bootstrap_result.provider_type;
    state_->api_key_ = std::move(bootstrap_result.api_key);
    state_->model_ = bootstrap_result.model;
    state_->ollama_url_ = bootstrap_result.ollama_url;

    state_->initializeAPIClient();
}

// --- Analysis Methods ---

AnalysisResult LLMEngine::analyze(const AnalysisInput& input,
                                  std::string_view analysis_type,
                                  const RequestOptions& options) {
    if (state_->metrics_collector_) {
        // Record latency
        auto start = std::chrono::high_resolution_clock::now();

        // Execute original analyze
        nlohmann::json input_json = input.toJson();
        // Extract system prompt if present to optimize/override if needed,
        // but explicit input usually overrides.
        // For now, we perform the direct call.

        auto result = analyze(input.system_prompt, input_json, analysis_type, options);

        auto end = std::chrono::high_resolution_clock::now();
        long latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::vector<MetricTag> tags = {{"provider", getProviderName()},
                                       {"model", getModelName()},
                                       {"type", std::string(analysis_type)},
                                       {"success", result.success ? "true" : "false"}};
        state_->metrics_collector_->recordLatency("llm_engine.analyze", latency, tags);

        if (result.success) {
            state_->metrics_collector_->recordCounter(
                "llm_engine.tokens_input", result.usage.promptTokens, tags);
            state_->metrics_collector_->recordCounter(
                "llm_engine.tokens_output", result.usage.completionTokens, tags);
        } else {
            state_->metrics_collector_->recordCounter("llm_engine.errors", 1, tags);
        }

        return result;
    }

    // No metrics, direct pass-through
    // Note: AnalysisInput might separate system_prompt.
    // The legacy analyze() takes prompt argument as the main prompt.
    // Should we pass input.user_message as prompt?
    // Yes, usually the first arg is the "prompt" (user message).

    std::string_view effective_prompt = input.user_message;
    if (effective_prompt.empty() && !input.system_prompt.empty()) {
        // Fallback or specific logic?
        // For now, assume user_message is the primary prompt.
    }

    return analyze(effective_prompt, input.toJson(), analysis_type, options);
}

AnalysisResult LLMEngine::analyze(std::string_view prompt,
                                  const nlohmann::json& input,
                                  std::string_view analysis_type,
                                  const RequestOptions& options) {
    state_->ensureSecureTmpDir();

    // Determine mode and instruction from options (if extended later, for now just defaults)
    std::string mode = "chat";
    bool prepend_terse_instruction = true;

    // Build context
    RequestContext ctx = RequestContextBuilder::build(static_cast<IModelContext&>(*this),
                                                      prompt,
                                                      input,
                                                      analysis_type,
                                                      mode,
                                                      prepend_terse_instruction);

    // Run Interceptors (onRequest)
    {
        std::shared_lock lock(state_->config_mutex_);
        for (const auto& interceptor : state_->interceptors_) {
            interceptor->onRequest(ctx);
        }
    }



    // Merge RequestOptions into finalParams
    // Merge RequestOptions into finalParams
    ParameterMerger::mergeRequestOptions(ctx.finalParams, options);


    // Apply options overrides like timeout/headers would go here if IRequestExecutor supported them
    // For now, we just pass parameters.
    // Ideally, IRequestExecutor interface needs expansion to support Per-Request Options.
    // Iteration 1 limitation: DefaultRequestExecutor might not support dynamic timeout override yet.
    // We will just execute.

    LLMAPI::APIResponse api_response;
    AnalysisResult result; // Declare result early for error paths? No, strictly following flow.

    if (state_->request_executor_) {
        api_response = state_->request_executor_->execute(
            state_->api_client_.get(), ctx.fullPrompt, input, ctx.finalParams, options);
    } else {
        if (state_->logger_) {
            state_->logger_->log(LogLevel::Error, "Request executor not configured.");
        }
        result = AnalysisResult{.success = false,
                                .think = "",
                                .content = "",
                                .finishReason = "",
                                .errorMessage = "Internal Error: Request executor missing",
                                .statusCode = HttpStatus::INTERNAL_SERVER_ERROR,
                                .usage = {},
                                .logprobs = std::nullopt,
                                .errorCode = LLMEngineErrorCode::Unknown,
                                .tool_calls = {}};
        // Should we run onResponse for errors? Yes, usually.
        {
            std::shared_lock lock(state_->config_mutex_);
            for (const auto& interceptor : state_->interceptors_) {
                interceptor->onResponse(result);
            }
        }
        return result;
    }

    result = ResponseHandler::handle(api_response,
                                     ctx.debugManager.get(),
                                     ctx.requestTmpDir,
                                     analysis_type,
                                     ctx.writeDebugFiles,
                                     state_->logger_.get());

    // Run Interceptors (onResponse)
    {
        std::shared_lock lock(state_->config_mutex_);
        for (const auto& interceptor : state_->interceptors_) {
            interceptor->onResponse(result);
        }
    }

    return result;
}

AnalysisResult LLMEngine::analyze(std::string_view prompt,
                                  const nlohmann::json& input,
                                  std::string_view analysis_type,
                                  std::string_view mode,
                                  bool prepend_terse_instruction) {
    // Forward to options-based overload?
    // Actually, the options overload lacks mode/prepend... arguments in my struct logic.
    // The previous implementation of overload was:
    // RequestOptions has timeout/retries.
    // Mode/Prepend are semantic args.
    // This is a bit conflicting.
    // Let's keep the original implementation logic here, but using state_.

    state_->ensureSecureTmpDir();

    RequestContext ctx = RequestContextBuilder::build(static_cast<IModelContext&>(*this),
                                                      prompt,
                                                      input,
                                                      analysis_type,
                                                      mode,
                                                      prepend_terse_instruction);

    LLMAPI::APIResponse api_response;
    if (state_->request_executor_) {
        api_response = state_->request_executor_->execute(
            state_->api_client_.get(), ctx.fullPrompt, input, ctx.finalParams);
    } else {
        return AnalysisResult{.success = false,
                              .think = "",
                              .content = "",
                              .finishReason = "",
                              .errorMessage = "Internal Error: Request executor missing",
                              .statusCode = HttpStatus::INTERNAL_SERVER_ERROR,
                              .usage = {},
                              .logprobs = std::nullopt,
                              .errorCode = LLMEngineErrorCode::Unknown,
                              .tool_calls = {}};
    }

    return ResponseHandler::handle(api_response,
                                   ctx.debugManager.get(),
                                   ctx.requestTmpDir,
                                   analysis_type,
                                   ctx.writeDebugFiles,
                                   state_->logger_.get());
}

std::future<AnalysisResult> LLMEngine::analyzeAsync(std::string_view prompt,
                                                    const nlohmann::json& input,
                                                    std::string_view analysis_type,
                                                    std::string_view mode,
                                                    bool prepend_terse_instruction) {
    // Copy shared state pointer. Efficiently captures the PIMPL state,
    // ensuring the engine's internal components remain valid for the duration
    // of the async task, even if the LLMEngine instance itself is destroyed.
    std::shared_ptr<EngineState> state = state_;

    return std::async(
        std::launch::async,
        [state /* captured shared state */,
         prompt = std::string(prompt),
         input = input,
         analysis_type = std::string(analysis_type),
         mode = std::string(mode),
         prepend_terse_instruction]() -> AnalysisResult {
            struct StateAdapter : public IModelContext {
                std::shared_ptr<EngineState> s;
                StateAdapter(std::shared_ptr<EngineState> st) : s(st) {}
                std::string getTempDirectory() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->tmp_dir_;
                }
                std::shared_ptr<IPromptBuilder> getTersePromptBuilder() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->terse_prompt_builder_;
                }
                std::shared_ptr<IPromptBuilder> getPassthroughPromptBuilder() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->passthrough_prompt_builder_;
                }
                const nlohmann::json& getModelParams() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->model_params_;
                }
                bool areDebugFilesEnabled() const override {
                    std::shared_lock lock(s->config_mutex_);
                    if (!s->debug_)
                        return false;
                    if (s->debug_files_policy_)
                        return s->debug_files_policy_();
                    return !s->disable_debug_files_env_cached_;
                }
                std::shared_ptr<IArtifactSink> getArtifactSink() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->artifact_sink_;
                }
                int getLogRetentionHours() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->log_retention_hours_;
                }
                std::shared_ptr<Logger> getLogger() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->logger_;
                }
                void prepareTempDirectory() override {
                    s->ensureSecureTmpDir();
                }
            };

            StateAdapter adapter(state);
            state->ensureSecureTmpDir();

            RequestContext ctx = RequestContextBuilder::build(
                adapter, prompt, input, analysis_type, mode, prepend_terse_instruction);

            LLMAPI::APIResponse api_response;
            if (state->request_executor_) {
                api_response = state->request_executor_->execute(
                    state->api_client_.get(), ctx.fullPrompt, input, ctx.finalParams);
            } else {
                return AnalysisResult{.success = false,
                                      .think = "",
                                      .content = "",
                                      .finishReason = "",
                                      .errorMessage = "Internal Error",
                                      .statusCode = HttpStatus::INTERNAL_SERVER_ERROR,
                                      .usage = {},
                                      .logprobs = std::nullopt,
                                      .errorCode = LLMEngineErrorCode::Unknown,
                                      .tool_calls = {}};
            }

            return ResponseHandler::handle(api_response,
                                           ctx.debugManager.get(),
                                           ctx.requestTmpDir,
                                           analysis_type,
                                           ctx.writeDebugFiles,
                                           state->logger_.get());
        });
}



std::future<AnalysisResult> LLMEngine::analyzeAsync(std::string_view prompt,
                                                    const nlohmann::json& input,
                                                    std::string_view analysis_type,
                                                    const RequestOptions& options) {
    // Copy shared state pointer. Efficiently captures the PIMPL state,
    // ensuring the engine's internal components remain valid for the duration
    // of the async task, even if the LLMEngine instance itself is destroyed.
    std::shared_ptr<EngineState> state = state_;

    // Default mode/instruction for options-based overload
    std::string mode = "chat";
    bool prepend_terse_instruction = true;

    return std::async(
        std::launch::async,
        [state /* captured shared state */,
         prompt = std::string(prompt),
         input = input,
         analysis_type = std::string(analysis_type),
         mode,
         prepend_terse_instruction,
         options]() -> AnalysisResult {
            auto start = std::chrono::high_resolution_clock::now();

            struct StateAdapter : public IModelContext {
                std::shared_ptr<EngineState> s;
                StateAdapter(std::shared_ptr<EngineState> st) : s(st) {}
                std::string getTempDirectory() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->tmp_dir_;
                }
                std::shared_ptr<IPromptBuilder> getTersePromptBuilder() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->terse_prompt_builder_;
                }
                std::shared_ptr<IPromptBuilder> getPassthroughPromptBuilder() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->passthrough_prompt_builder_;
                }
                const nlohmann::json& getModelParams() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->model_params_;
                }
                bool areDebugFilesEnabled() const override {
                    std::shared_lock lock(s->config_mutex_);
                    if (!s->debug_)
                        return false;
                    if (s->debug_files_policy_)
                        return s->debug_files_policy_();
                    return !s->disable_debug_files_env_cached_;
                }
                std::shared_ptr<IArtifactSink> getArtifactSink() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->artifact_sink_;
                }
                int getLogRetentionHours() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->log_retention_hours_;
                }
                std::shared_ptr<Logger> getLogger() const override {
                    std::shared_lock lock(s->config_mutex_);
                    return s->logger_;
                }
                void prepareTempDirectory() override {
                    s->ensureSecureTmpDir();
                }
            };

            StateAdapter adapter(state);
            state->ensureSecureTmpDir();

            RequestContext ctx = RequestContextBuilder::build(
                adapter, prompt, input, analysis_type, mode, prepend_terse_instruction);

            // Run Interceptors (onRequest)
            {
                std::shared_lock lock(state->config_mutex_);
                for (const auto& interceptor : state->interceptors_) {
                    interceptor->onRequest(ctx);
                }
            }



            // Merge RequestOptions into finalParams
            // Merge RequestOptions into finalParams
            ParameterMerger::mergeRequestOptions(ctx.finalParams, options);


            LLMAPI::APIResponse api_response;
            if (state->request_executor_) {
                api_response = state->request_executor_->execute(
                    state->api_client_.get(), ctx.fullPrompt, input, ctx.finalParams, options);
            } else {
                AnalysisResult errResult{.success = false,
                                         .think = "",
                                         .content = "",
                                         .finishReason = "",
                                         .errorMessage = "Internal Error",
                                         .statusCode = HttpStatus::INTERNAL_SERVER_ERROR,
                                         .usage = {},
                                         .logprobs = std::nullopt,
                                         .errorCode = LLMEngineErrorCode::Unknown,
                                         .tool_calls = {}};
                for (const auto& interceptor : state->interceptors_) {
                    interceptor->onResponse(errResult);
                }
                return errResult;
            }

            auto result = ResponseHandler::handle(api_response,
                                                  ctx.debugManager.get(),
                                                  ctx.requestTmpDir,
                                                  analysis_type,
                                                  ctx.writeDebugFiles,
                                                  state->logger_.get());

            // Run Interceptors (onResponse)
            {
                std::shared_lock lock(state->config_mutex_);
                for (const auto& interceptor : state->interceptors_) {
                    interceptor->onResponse(result);
                }
            }

            // Record Metrics
            if (state->metrics_collector_) {
                auto end = std::chrono::high_resolution_clock::now();
                long latency =
                    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                std::string provider =
                    state->api_client_ ? state->api_client_->getProviderName() : "Ollama (Legacy)";
                std::vector<MetricTag> tags = {{"provider", provider},
                                               {"model", state->model_},
                                               {"type", analysis_type},
                                               {"success", result.success ? "true" : "false"},
                                               {"mode", "async"}};

                state->metrics_collector_->recordLatency("llm_engine.analyze", latency, tags);

                if (result.success) {
                    state->metrics_collector_->recordCounter(
                        "llm_engine.tokens_input", result.usage.promptTokens, tags);
                    state->metrics_collector_->recordCounter(
                        "llm_engine.tokens_output", result.usage.completionTokens, tags);
                } else {
                    state->metrics_collector_->recordCounter("llm_engine.errors", 1, tags);
                }
            }

            return result;
        });
}

void LLMEngine::analyzeStream(std::string_view prompt,
                              const nlohmann::json& input,
                              std::string_view analysis_type,
                              const RequestOptions& options,
                              StreamCallback callback) {
    state_->ensureSecureTmpDir();

    std::string mode = "chat";
    bool prepend_terse_instruction = true;

    RequestContext ctx = RequestContextBuilder::build(static_cast<IModelContext&>(*this),
                                                      prompt,
                                                      input,
                                                      analysis_type,
                                                      mode,
                                                      prepend_terse_instruction);



    // Merge RequestOptions into finalParams
    // Merge RequestOptions into finalParams
    ParameterMerger::mergeRequestOptions(ctx.finalParams, options);


    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Use shared pointer to accumulate usage across chunks
    auto usage_accum = std::make_shared<std::optional<AnalysisResult::UsageStats>>();

    auto wrapped_callback = [callback, this, start_time, analysis_type, usage_accum](const StreamChunk& chunk) {
        // Accumulate usage if present in this chunk
        if (chunk.usage.has_value()) {
            *usage_accum = chunk.usage;
        }
        
        callback(chunk);

        if (chunk.is_done && state_->metrics_collector_) {
            auto end = std::chrono::high_resolution_clock::now();
            long latency =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start_time).count();
            std::vector<MetricTag> tags = {
                {"provider", getProviderName()},
                {"model", getModelName()},
                {"type", std::string(analysis_type)},
                {"success", chunk.error_code == LLMEngineErrorCode::None ? "true" : "false"},
                {"mode", "stream"}};
            state_->metrics_collector_->recordLatency("llm_engine.analyze", latency, tags);

            if (usage_accum->has_value()) {
                const auto& usage = **usage_accum;
                state_->metrics_collector_->recordCounter(
                    "llm_engine.tokens_input", usage.promptTokens, tags);
                state_->metrics_collector_->recordCounter(
                    "llm_engine.tokens_output", usage.completionTokens, tags);
            }
            // Error counting? If chunk.error_code is set.
            if (chunk.error_code != LLMEngineErrorCode::None) {
                state_->metrics_collector_->recordCounter("llm_engine.errors", 1, tags);
            }
        }
    };


    if (state_->request_executor_) {
        // executeStream now takes StreamCallback directly
        state_->request_executor_->executeStream(state_->api_client_.get(),
                                                 ctx.fullPrompt,
                                                 input,
                                                 ctx.finalParams,
                                                 wrapped_callback,
                                                 options);
    } else if (state_->api_client_) {
        if (state_->logger_) {
            state_->logger_->log(LogLevel::Warn,
                                 "Request executor not set, falling back to direct client usage.");
        }
        state_->api_client_->sendRequestStream(
            ctx.fullPrompt, input, ctx.finalParams, wrapped_callback, options);
    } else {
        if (state_->logger_) {
            state_->logger_->log(LogLevel::Error, "API client not initialized for streaming.");
        }
        StreamChunk errChunk;
        errChunk.is_done = true;
        errChunk.error_code = LLMEngineErrorCode::Unknown;
        errChunk.error_message = "API client/Executor not initialized";
        wrapped_callback(errChunk);
    }
}

std::vector<AnalysisResult> LLMEngine::analyzeBatch(const std::vector<AnalysisInput>& inputs,
                                                    std::string_view analysis_type,
                                                    const RequestOptions& options) {
    if (inputs.empty()) {
        return {};
    }

    // Set up concurrency limiter
    // Default concurrency logic: use hardware concurrency or reasonable default (16)
    size_t concurrency = 16;
    if (options.max_concurrency.has_value() && *options.max_concurrency > 0) {
        concurrency = *options.max_concurrency;
    } else {
        size_t hw = std::thread::hardware_concurrency();
        if (hw > 0) concurrency = hw * 2;
    }

    Utils::ThreadPool pool(concurrency);
    std::vector<std::future<AnalysisResult>> futures;
    futures.reserve(inputs.size());

    for (const auto& input : inputs) {
        futures.push_back(
            pool.enqueue([this, input, analysis_type, options]() {
                return this->analyze(input, analysis_type, options);
            }));
    }

    std::vector<AnalysisResult> results;
    results.reserve(inputs.size());
    for (auto& f : futures) {
        results.push_back(f.get());
    }
    return results;
}

void LLMEngine::addInterceptor(std::shared_ptr<IInterceptor> interceptor) {
    if (interceptor) {
        std::unique_lock lock(state_->config_mutex_);
        state_->interceptors_.push_back(std::move(interceptor));
    }
}

// --- Accessors ---

std::shared_ptr<CancellationToken> LLMEngine::createCancellationToken() {
    return CancellationToken::create();
}

std::string LLMEngine::getProviderName() const {
    if (state_->api_client_) {
        return state_->api_client_->getProviderName();
    }
    return "Ollama (Legacy)";
}

std::string LLMEngine::getModelName() const {
    return state_->model_;
}
LLMAPI::ProviderType LLMEngine::getProviderType() const {
    return state_->provider_type_;
}

std::string LLMEngine::getBackendType() const {
    return LLMAPI::APIClientFactory::providerTypeToString(state_->provider_type_);
}

bool LLMEngine::isOnlineProvider() const {
    return state_->provider_type_ != LLMAPI::ProviderType::OLLAMA;
}

std::string LLMEngine::getTempDirectory() const {
    return state_->tmp_dir_;
}
std::shared_ptr<IPromptBuilder> LLMEngine::getTersePromptBuilder() const {
    return state_->terse_prompt_builder_;
}
std::shared_ptr<IPromptBuilder> LLMEngine::getPassthroughPromptBuilder() const {
    return state_->passthrough_prompt_builder_;
}
const nlohmann::json& LLMEngine::getModelParams() const {
    return state_->model_params_;
}
bool LLMEngine::areDebugFilesEnabled() const {
    if (!state_->debug_)
        return false;
    if (state_->debug_files_policy_)
        return state_->debug_files_policy_();
    return !state_->disable_debug_files_env_cached_;
}
std::shared_ptr<IArtifactSink> LLMEngine::getArtifactSink() const {
    return state_->artifact_sink_;
}
int LLMEngine::getLogRetentionHours() const {
    return state_->log_retention_hours_;
}
std::shared_ptr<Logger> LLMEngine::getLogger() const {
    return state_->logger_;
}
void LLMEngine::prepareTempDirectory() {
    state_->ensureSecureTmpDir();
}

bool LLMEngine::isDebugEnabled() const {
    return state_->debug_;
}

bool LLMEngine::setTempDirectory(const std::string& tmp_dir) {
    const std::string allowed_root = state_->temp_dir_provider_
                                         ? state_->temp_dir_provider_->getTempDir()
                                         : DefaultTempDirProvider().getTempDir();

    if (TempDirectoryService::validatePathWithinRoot(
            tmp_dir, allowed_root, state_->logger_.get())) {
        state_->tmp_dir_ = tmp_dir;
        state_->tmp_dir_verified_ = false;
        return true;
    }
    return false;
}

void LLMEngine::setLogger(std::shared_ptr<Logger> logger) {
    if (logger)
        state_->logger_ = std::move(logger);
}

void LLMEngine::setDebugFilesPolicy(std::function<bool()> policy) {
    state_->debug_files_policy_ = std::move(policy);
}

void LLMEngine::setDebugFilesEnabled(bool enabled) {
    state_->debug_files_policy_ = [enabled]() { return enabled; };
}

void LLMEngine::setRequestExecutor(std::shared_ptr<IRequestExecutor> executor) {
    if (executor)
        state_->request_executor_ = std::move(executor);
}

void LLMEngine::setArtifactSink(std::shared_ptr<IArtifactSink> sink) {
    if (sink)
        state_->artifact_sink_ = std::move(sink);
}

void LLMEngine::setPromptBuilders(std::shared_ptr<IPromptBuilder> terse,
                                  std::shared_ptr<IPromptBuilder> passthrough) {
    if (terse)
        state_->terse_prompt_builder_ = std::move(terse);
    if (passthrough)
        state_->passthrough_prompt_builder_ = std::move(passthrough);
}

void LLMEngine::setMetricsCollector(std::shared_ptr<IMetricsCollector> collector) {
    state_->metrics_collector_ = std::move(collector);
}

} // namespace LLMEngine
