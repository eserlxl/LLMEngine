// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/LLMEngine.hpp"
#include "EngineState.hpp"

#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/AnalysisInput.hpp"

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

#include "LLMEngine/Utils/ThreadPool.hpp"

#include <chrono>
#include <mutex>
#include <shared_mutex>



#include <cstdlib>

#include <stdexcept>
#include <string>

// Namespace aliases to reduce verbosity
namespace LLM = ::LLMEngine;
namespace LLMAPI = ::LLMEngineAPI;



namespace LLMEngine {



// Internal state structure implementation

namespace {
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
} // namespace


// --- Constructors ---

LLMEngine::LLMEngine(std::unique_ptr<LLMAPI::APIClient> client,
                     const nlohmann::json& modelParams,
                     int logRetentionHours,
                     bool debug,
                     const std::shared_ptr<ITempDirProvider>& tempDirProvider) {
    state_ = std::make_shared<EngineState>(modelParams, logRetentionHours, debug);

    if (tempDirProvider) {
        state_->tempDirProvider_ = tempDirProvider;
        state_->tmp_dir_ = tempDirProvider->getTempDir();
    }

    if (!client) {
        throw std::runtime_error("API client must not be null");
    }
    state_->apiClient_ = std::move(client);
    state_->providerType_ = state_->apiClient_->getProviderType();
}

LLMEngine::LLMEngine(LLMAPI::ProviderType providerType,
                     std::string_view apiKey,
                     std::string_view model,
                     const nlohmann::json& modelParams,
                     int logRetentionHours,
                     bool debug) {
    state_ = std::make_shared<EngineState>(modelParams, logRetentionHours, debug);
    state_->providerType_ = providerType;

    // Resolve API key using ProviderBootstrap (respects environment variables)
    state_->apiKey_ =
        ProviderBootstrap::resolveApiKey(providerType, apiKey, "", state_->logger_.get());

    state_->model_ = std::string(model);
    state_->configManager_ = std::shared_ptr<LLMAPI::IConfigManager>(
        &LLMAPI::APIConfigManager::getInstance(), [](LLMAPI::IConfigManager*) {});

    state_->initializeAPIClient();
}

LLMEngine::LLMEngine(std::string_view providerName,
                     std::string_view apiKey,
                     std::string_view model,
                     const nlohmann::json& modelParams,
                     int logRetentionHours,
                     bool debug,
                     const std::shared_ptr<LLMAPI::IConfigManager>& configManager,
                     std::string_view baseUrl) {
    state_ = std::make_shared<EngineState>(modelParams, logRetentionHours, debug);

    // Manage config manager
    if (configManager) {
        state_->configManager_ = configManager;
    } else {
        state_->configManager_ = std::shared_ptr<LLMAPI::IConfigManager>(
            &LLMAPI::APIConfigManager::getInstance(), [](LLMAPI::IConfigManager*) {});
    }

    auto bootstrapResult = ProviderBootstrap::bootstrap(
        providerName, apiKey, model, state_->configManager_, state_->logger_.get());

    state_->providerType_ = bootstrapResult.providerType;
    state_->apiKey_ = std::move(bootstrapResult.apiKey);
    state_->model_ = bootstrapResult.model;
    
    // Use bootstrapped URL unless explicit baseUrl is provided
    if (!baseUrl.empty()) {
        state_->ollamaUrl_ = std::string(baseUrl);
    } else {
        state_->ollamaUrl_ = bootstrapResult.ollamaUrl;
    }

    state_->initializeAPIClient();
}

// --- Analysis Methods ---

void LLMEngine::setDefaultRequestOptions(const RequestOptions& options) {
    std::unique_lock lock(state_->configMutex_);
    state_->defaultRequestOptions_ = options;
}

void LLMEngine::validateInput(const AnalysisInput& input) const {
    std::string error;
    if (!input.validate(error)) {
        throw std::invalid_argument("Invalid AnalysisInput: " + error);
    }
}

AnalysisResult LLMEngine::analyze(const AnalysisInput& input,
                                  std::string_view analysisType,
                                  const RequestOptions& options) {
    validateInput(input);
    if (state_->metricsCollector_) {
        // Record latency
        auto start = std::chrono::high_resolution_clock::now();

        // Execute original analyze
        nlohmann::json inputJson = input.toJson();
        // Extract system prompt if present to optimize/override if needed,
        // but explicit input usually overrides.
        // For now, we perform the direct call.

        // MERGE OPTIONS
        RequestOptions effectiveOptions;
        {
            std::shared_lock lock(state_->configMutex_);
            effectiveOptions = RequestOptions::merge(state_->defaultRequestOptions_, options);
        }

        auto result = analyze(input.system_prompt, inputJson, analysisType, effectiveOptions);

        auto end = std::chrono::high_resolution_clock::now();
        long latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::vector<MetricTag> tags = {{"provider", getProviderName()},
                                       {"model", getModelName()},
                                       {"type", std::string(analysisType)},
                                       {"success", result.success ? "true" : "false"}};
        state_->metricsCollector_->recordLatency("llm_engine.analyze", latency, tags);

        if (result.success) {
            state_->metricsCollector_->recordCounter(
                "llm_engine.tokens_input", result.usage.promptTokens, tags);
            state_->metricsCollector_->recordCounter(
                "llm_engine.tokens_output", result.usage.completionTokens, tags);
        } else {
            state_->metricsCollector_->recordCounter("llm_engine.errors", 1, tags);
        }

        return result;
    }

    // No metrics, direct pass-through
    // Note: AnalysisInput might separate system_prompt.
    // The legacy analyze() takes prompt argument as the main prompt.
    // Should we pass input.user_message as prompt?
    // Yes, usually the first arg is the "prompt" (user message).

    std::string_view effectivePrompt = input.user_message;
    if (effectivePrompt.empty() && !input.system_prompt.empty()) {
        // Fallback or specific logic?
        // For now, assume user_message is the primary prompt.
    }

    RequestOptions effectiveOptions;
    {
        std::shared_lock lock(state_->configMutex_);
        effectiveOptions = RequestOptions::merge(state_->defaultRequestOptions_, options);
    }

    return analyze(effectivePrompt, input.toJson(), analysisType, effectiveOptions);
}

AnalysisResult LLMEngine::analyze(std::string_view prompt,
                                  const nlohmann::json& input,
                                  std::string_view analysisType,
                                  const RequestOptions& options) {
    validateInput(AnalysisInput::fromJson(input)); // Validate input structure
    state_->ensureSecureTmpDir();

    // Determine mode and instruction from options (if extended later, for now just defaults)
    std::string mode = "chat";
    bool prependTerseInstruction = true;

    // Build context
    RequestContext ctx = RequestContextBuilder::build(static_cast<IModelContext&>(*this),
                                                      prompt,
                                                      input,
                                                      analysisType,
                                                      mode,
                                                      prependTerseInstruction);

    // Run Interceptors (onRequest)
    {
        std::shared_lock lock(state_->configMutex_);
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

    LLMAPI::APIResponse apiResponse;
    AnalysisResult result; // Declare result early for error paths? No, strictly following flow.

    if (state_->requestExecutor_) {
        apiResponse = state_->requestExecutor_->execute(
            state_->apiClient_.get(), ctx.fullPrompt, input, ctx.finalParams, options);
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
            std::shared_lock lock(state_->configMutex_);
            for (const auto& interceptor : state_->interceptors_) {
                interceptor->onResponse(result);
            }
        }
        return result;
    }

    result = ResponseHandler::handle(apiResponse,
                                     ctx.debugManager.get(),
                                     ctx.requestTmpDir,
                                     analysisType,
                                     ctx.writeDebugFiles,
                                     state_->logger_.get());

    // Run Interceptors (onResponse)
    {
        std::shared_lock lock(state_->configMutex_);
        for (const auto& interceptor : state_->interceptors_) {
            interceptor->onResponse(result);
        }
    }

    return result;
}

AnalysisResult LLMEngine::analyze(std::string_view prompt,
                                  const nlohmann::json& input,
                                  std::string_view analysisType,
                                  std::string_view mode,
                                  bool prependTerseInstruction) {
    // Forward to options-based overload?
    // Actually, the options overload lacks mode/prepend... arguments in my struct logic.
    // The previous implementation of overload was:
    // RequestOptions has timeout/retries.
    // Mode/Prepend are semantic args.
    // This is a bit conflicting.
    // Let's keep the original implementation logic here, but using state_.
    
    validateInput(AnalysisInput::fromJson(input)); // Validate input structure

    state_->ensureSecureTmpDir();

    RequestContext ctx = RequestContextBuilder::build(static_cast<IModelContext&>(*this),
                                                      prompt,
                                                      input,
                                                      analysisType,
                                                      mode,
                                                      prependTerseInstruction);

    LLMAPI::APIResponse apiResponse;
    if (state_->requestExecutor_) {
        apiResponse = state_->requestExecutor_->execute(
            state_->apiClient_.get(), ctx.fullPrompt, input, ctx.finalParams);
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

    return ResponseHandler::handle(apiResponse,
                                   ctx.debugManager.get(),
                                   ctx.requestTmpDir,
                                   analysisType,
                                   ctx.writeDebugFiles,
                                   state_->logger_.get());
}

std::future<AnalysisResult> LLMEngine::analyzeAsync(std::string_view prompt,
                                                    const nlohmann::json& input,
                                                    std::string_view analysisType,
                                                    std::string_view mode,
                                                    bool prependTerseInstruction) {
    // Copy shared state pointer. Efficiently captures the PIMPL state,
    // ensuring the engine's internal components remain valid for the duration
    // of the async task, even if the LLMEngine instance itself is destroyed.
    std::shared_ptr<EngineState> state = state_;

    return std::async(
        std::launch::async,
        [state /* captured shared state */,
         prompt = std::string(prompt),
         input = input,
         analysisType = std::string(analysisType),
         mode = std::string(mode),
         prependTerseInstruction]() -> AnalysisResult {
            EngineStateAdapter adapter(state);
            state->ensureSecureTmpDir();

            RequestContext ctx = RequestContextBuilder::build(
                adapter, prompt, input, analysisType, mode, prependTerseInstruction);

            LLMAPI::APIResponse apiResponse;
            if (state->requestExecutor_) {
                apiResponse = state->requestExecutor_->execute(
                    state->apiClient_.get(), ctx.fullPrompt, input, ctx.finalParams);
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

            return ResponseHandler::handle(apiResponse,
                                           ctx.debugManager.get(),
                                           ctx.requestTmpDir,
                                           analysisType,
                                           ctx.writeDebugFiles,
                                           state->logger_.get());
        });
}



std::future<AnalysisResult> LLMEngine::analyzeAsync(std::string_view prompt,
                                                    const nlohmann::json& input,
                                                    std::string_view analysisType,
                                                    const RequestOptions& options) {
    validateInput(AnalysisInput::fromJson(input));
    // Copy shared state pointer. Efficiently captures the PIMPL state,
    // ensuring the engine's internal components remain valid for the duration
    // of the async task, even if the LLMEngine instance itself is destroyed.
    std::shared_ptr<EngineState> state = state_;

    // Merge Options immediately (capture current state)
    RequestOptions effectiveOptions;
    {
        std::shared_lock lock(state->configMutex_);
        effectiveOptions = RequestOptions::merge(state->defaultRequestOptions_, options);
    }

    // Default mode/instruction for options-based overload
    std::string mode = "chat";
    bool prependTerseInstruction = true;

    return std::async(
        std::launch::async,
        [state /* captured shared state */,
         prompt = std::string(prompt),
         input = input,
         analysisType = std::string(analysisType),
         mode = std::string(mode),
         prependTerseInstruction,
         options=effectiveOptions]() -> AnalysisResult {
            auto start = std::chrono::high_resolution_clock::now();

            EngineStateAdapter adapter(state);
            state->ensureSecureTmpDir();

            RequestContext ctx = RequestContextBuilder::build(
                adapter, prompt, input, analysisType, mode, prependTerseInstruction);

            // Run Interceptors (onRequest)
            {
                std::shared_lock lock(state->configMutex_);
                for (const auto& interceptor : state->interceptors_) {
                    interceptor->onRequest(ctx);
                }
            }



            // Merge RequestOptions into finalParams
            // Merge RequestOptions into finalParams
            ParameterMerger::mergeRequestOptions(ctx.finalParams, options);


            LLMAPI::APIResponse apiResponse;
            if (state->requestExecutor_) {
                apiResponse = state->requestExecutor_->execute(
                    state->apiClient_.get(), ctx.fullPrompt, input, ctx.finalParams, options);
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

            auto result = ResponseHandler::handle(apiResponse,
                                                  ctx.debugManager.get(),
                                                  ctx.requestTmpDir,
                                                  analysisType,
                                                  ctx.writeDebugFiles,
                                                  state->logger_.get());

            // Run Interceptors (onResponse)
            {
                std::shared_lock lock(state->configMutex_);
                for (const auto& interceptor : state->interceptors_) {
                    interceptor->onResponse(result);
                }
            }

            // Record Metrics
            if (state->metricsCollector_) {
                auto end = std::chrono::high_resolution_clock::now();
                long latency =
                    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                std::string provider =
                    state->apiClient_ ? state->apiClient_->getProviderName() : "Ollama (Legacy)";
                std::vector<MetricTag> tags = {{"provider", provider},
                                               {"model", state->model_},
                                               {"type", analysisType},
                                               {"success", result.success ? "true" : "false"},
                                               {"mode", "async"}};

                state->metricsCollector_->recordLatency("llm_engine.analyze", latency, tags);

                if (result.success) {
                    state->metricsCollector_->recordCounter(
                        "llm_engine.tokens_input", result.usage.promptTokens, tags);
                    state->metricsCollector_->recordCounter(
                        "llm_engine.tokens_output", result.usage.completionTokens, tags);
                } else {
                    state->metricsCollector_->recordCounter("llm_engine.errors", 1, tags);
                }
            }

            return result;
        });
}

void LLMEngine::analyzeStream(std::string_view prompt,
                               const nlohmann::json& input,
                               std::string_view analysisType,
                               const RequestOptions& options,
                               StreamCallback callback) {
    state_->ensureSecureTmpDir();

    RequestOptions effectiveOptions;
    {
        std::shared_lock lock(state_->configMutex_);
        effectiveOptions = RequestOptions::merge(state_->defaultRequestOptions_, options);
    }

    validateInput(AnalysisInput::fromJson(input));

    std::string mode = "chat";
    bool prependTerseInstruction = true;

    RequestContext ctx = RequestContextBuilder::build(static_cast<IModelContext&>(*this),
                                                      prompt,
                                                      input,
                                                      analysisType,
                                                      mode,
                                                      prependTerseInstruction);

    // Run Interceptors (onRequest)
    {
        std::shared_lock lock(state_->configMutex_);
        for (const auto& interceptor : state_->interceptors_) {
            interceptor->onRequest(ctx);
        }
    }


    // Merge RequestOptions into finalParams
    // Merge RequestOptions into finalParams
    // Merge RequestOptions into finalParams
    // Merge RequestOptions into finalParams
    ParameterMerger::mergeRequestOptions(ctx.finalParams, effectiveOptions);


    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Use shared pointer to accumulate usage across chunks
    auto usageAccum = std::make_shared<std::optional<AnalysisResult::UsageStats>>();

    auto wrappedCallback = [callback, this, startTime, analysisType, usageAccum](const StreamChunk& chunk) {
        // Accumulate usage if present in this chunk
        if (chunk.usage.has_value()) {
            *usageAccum = chunk.usage;
        }
        
        callback(chunk);

        if (chunk.is_done && state_->metricsCollector_) {
            auto end = std::chrono::high_resolution_clock::now();
            long latency =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
            std::vector<MetricTag> tags = {
                {"provider", getProviderName()},
                {"model", getModelName()},
                {"type", std::string(analysisType)},
                {"success", chunk.error_code == LLMEngineErrorCode::None ? "true" : "false"},
                {"mode", "stream"}};
            state_->metricsCollector_->recordLatency("llm_engine.analyze", latency, tags);

            if (usageAccum->has_value()) {
                const auto& usage = **usageAccum;
                state_->metricsCollector_->recordCounter(
                    "llm_engine.tokens_input", usage.promptTokens, tags);
                state_->metricsCollector_->recordCounter(
                    "llm_engine.tokens_output", usage.completionTokens, tags);
            }
            // Error counting? If chunk.error_code is set.
            if (chunk.error_code != LLMEngineErrorCode::None) {
                state_->metricsCollector_->recordCounter("llm_engine.errors", 1, tags);
            }
        }
    };


    if (state_->requestExecutor_) {
        state_->requestExecutor_->executeStream(state_->apiClient_.get(),
                                                 ctx.fullPrompt,
                                                 input,
                                                 ctx.finalParams,
                                                 wrappedCallback,
                                                 effectiveOptions);
    } else if (state_->apiClient_) {
        if (state_->logger_) {
            state_->logger_->log(LogLevel::Warn,
                                 "Request executor not set, falling back to direct client usage.");
        }
        state_->apiClient_->sendRequestStream(
            ctx.fullPrompt, input, ctx.finalParams, wrappedCallback, effectiveOptions);
    } else {
        if (state_->logger_) {
            state_->logger_->log(LogLevel::Error, "API client not initialized for streaming.");
        }
        StreamChunk errChunk;
        errChunk.is_done = true;
        errChunk.error_code = LLMEngineErrorCode::Unknown;
        errChunk.error_message = "API client/Executor not initialized";
        errChunk.error_message = "API client/Executor not initialized";
        wrappedCallback(errChunk);
    }

    // Run Interceptors (onResponse) - approximated for stream
    // Note: Streaming makes full onResponse difficult as result is built incrementally.
    // We can trigger it if we accumulated a full result or just skip for now?
    // Given usageAccum logic, we could potentially construct a partial result?
    // For now, skipping full onResponse for stream to avoid complexity,
    // as IInterceptor::onResponse takes AnalysisResult&.
}

std::vector<AnalysisResult> LLMEngine::analyzeBatch(const std::vector<AnalysisInput>& inputs,
                                                    std::string_view analysisType,
                                                    const RequestOptions& options) {
    if (inputs.empty()) {
        return {};
    }



    // Validate all inputs first
    for (const auto& input : inputs) {
        validateInput(input);
    }

    RequestOptions effectiveOptions;
    {
        std::shared_lock lock(state_->configMutex_);
        effectiveOptions = RequestOptions::merge(state_->defaultRequestOptions_, options);
    }

    // Set up concurrency limiter
    // Default concurrency logic: use hardware concurrency or reasonable default (16)
    size_t concurrency = std::thread::hardware_concurrency();
    if (concurrency == 0) concurrency = 4; // Absolute fallback

    if (options.max_concurrency.has_value() && *options.max_concurrency > 0) {
        concurrency = *options.max_concurrency;
    }

    Utils::ThreadPool pool(concurrency);
    std::vector<std::future<AnalysisResult>> futures;
    futures.reserve(inputs.size());

    for (const auto& input : inputs) {
        futures.push_back(
            pool.enqueue([this, input, analysisType, effectiveOptions]() {
                return this->analyze(input, analysisType, effectiveOptions);
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
        std::unique_lock lock(state_->configMutex_);
        state_->interceptors_.push_back(std::move(interceptor));
    }
}

// --- Accessors ---

std::shared_ptr<CancellationToken> LLMEngine::createCancellationToken() {
    return CancellationToken::create();
}

std::string LLMEngine::getProviderName() const {
    if (state_->apiClient_) {
        return state_->apiClient_->getProviderName();
    }
    return "Ollama (Legacy)";
}

std::string LLMEngine::getModelName() const {
    return state_->model_;
}
LLMAPI::ProviderType LLMEngine::getProviderType() const {
    return state_->providerType_;
}

std::string LLMEngine::getBackendType() const {
    return LLMAPI::APIClientFactory::providerTypeToString(state_->providerType_);
}

bool LLMEngine::isOnlineProvider() const {
    return state_->providerType_ != LLMAPI::ProviderType::OLLAMA;
}

std::string LLMEngine::getTempDirectory() const {
    return state_->tmp_dir_;
}
std::shared_ptr<IPromptBuilder> LLMEngine::getTersePromptBuilder() const {
    return state_->tersePromptBuilder_;
}
std::shared_ptr<IPromptBuilder> LLMEngine::getPassthroughPromptBuilder() const {
    return state_->passthroughPromptBuilder_;
}
const nlohmann::json& LLMEngine::getModelParams() const {
    return state_->modelParams_;
}
bool LLMEngine::areDebugFilesEnabled() const {
    if (!state_->debug_)
        return false;
    if (state_->debugFilesPolicy_)
        return state_->debugFilesPolicy_();
    return !state_->disableDebugFilesEnvCached_;
}
std::shared_ptr<IArtifactSink> LLMEngine::getArtifactSink() const {
    return state_->artifactSink_;
}
int LLMEngine::getLogRetentionHours() const {
    return state_->logRetentionHours_;
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
    const std::string allowedRoot = state_->tempDirProvider_
                                         ? state_->tempDirProvider_->getTempDir()
                                         : DefaultTempDirProvider().getTempDir();

    if (TempDirectoryService::validatePathWithinRoot(
            tmp_dir, allowedRoot, state_->logger_.get())) {
        state_->tmp_dir_ = tmp_dir;
        state_->tempDirVerified_ = false;
        return true;
    }
    return false;
}

void LLMEngine::setLogger(std::shared_ptr<Logger> logger) {
    if (logger)
        state_->logger_ = std::move(logger);
}

void LLMEngine::setDebugFilesPolicy(std::function<bool()> policy) {
    state_->debugFilesPolicy_ = std::move(policy);
}

void LLMEngine::setDebugFilesEnabled(bool enabled) {
    state_->debugFilesPolicy_ = [enabled]() { return enabled; };
}

void LLMEngine::setRequestExecutor(std::shared_ptr<IRequestExecutor> executor) {
    if (executor)
        state_->requestExecutor_ = std::move(executor);
}

void LLMEngine::setArtifactSink(std::shared_ptr<IArtifactSink> sink) {
    if (sink)
        state_->artifactSink_ = std::move(sink);
}

void LLMEngine::setPromptBuilders(std::shared_ptr<IPromptBuilder> terse,
                                  std::shared_ptr<IPromptBuilder> passthrough) {
    if (terse)
        state_->tersePromptBuilder_ = std::move(terse);
    if (passthrough)
        state_->passthroughPromptBuilder_ = std::move(passthrough);
}

void LLMEngine::setMetricsCollector(std::shared_ptr<IMetricsCollector> collector) {
    state_->metricsCollector_ = std::move(collector);
}

} // namespace LLMEngine
