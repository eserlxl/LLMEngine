// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test support (fake API client) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "FakeAPIClient.hpp"
#include "LLMEngine/HttpStatus.hpp"

namespace LLMEngineAPI {

FakeAPIClient::FakeAPIClient(ProviderType type, std::string providerName)
    : provider_type_(type), provider_name_(std::move(providerName)) {}

void FakeAPIClient::setNextResponse(const APIResponse& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    next_response_ = response;
    has_custom_response_ = true;
}

APIResponse FakeAPIClient::sendRequest(std::string_view prompt,
                                       const nlohmann::json& input,
                                       const nlohmann::json& params,
                                       const ::LLMEngine::RequestOptions& options) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    last_options_ = options;
    last_input_ = input;
    last_params_ = params;
    last_prompt_ = std::string(prompt);
    if (has_custom_response_) {
        has_custom_response_ = false;
        return next_response_;
    }

    APIResponse r;
    r.success = true;
    // Produce a deterministic echo-style response for tests
    r.content = std::string("[FAKE] ") + std::string(prompt);
    r.status_code = ::LLMEngine::HttpStatus::OK;
    r.raw_response = {{"fake", true},
                      {"provider", provider_name_},
                      {"prompt_len", static_cast<int>(std::string(prompt).size())},
                      {"has_system", input.contains("system_prompt")}};
    return r;
}

void FakeAPIClient::setNextStreamChunks(const std::vector<std::string>& chunks) {
    std::lock_guard<std::mutex> lock(m_mutex);
    next_stream_chunks_ = chunks;
    has_custom_stream_ = true;
}

void FakeAPIClient::sendRequestStream(std::string_view prompt,
                                      const nlohmann::json& input,
                                      const nlohmann::json& params,
                                      LLMEngine::StreamCallback callback,
                                      const ::LLMEngine::RequestOptions& options) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    last_options_ = options;
    last_input_ = input;
    last_params_ = params;
    last_prompt_ = std::string(prompt);

    (void)params;

    if (has_custom_stream_) {
        has_custom_stream_ = false;
        for (const auto& chunk : next_stream_chunks_) {
            LLMEngine::StreamChunk stream_chunk;
            stream_chunk.content = chunk;
            stream_chunk.is_done = false;
            callback(stream_chunk);
        }
        // Send done signal
        LLMEngine::StreamChunk done_chunk;
        done_chunk.is_done = true;
        callback(done_chunk);
        return;
    }

    // Default behavior: one chunk with prompt
    std::string content = "[FAKE STREAM] " + std::string(prompt);
    LLMEngine::StreamChunk stream_chunk;
    stream_chunk.content = content;
    stream_chunk.is_done = false;
    callback(stream_chunk);
    
    LLMEngine::StreamChunk done_chunk;
    done_chunk.is_done = true;
    callback(done_chunk);
}

} // namespace LLMEngineAPI
