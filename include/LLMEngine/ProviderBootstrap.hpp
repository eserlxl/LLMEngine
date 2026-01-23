// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/LLMEngineExport.hpp"
#include "LLMEngine/Logger.hpp"

#include <memory>
#include <string>
#include <string_view>

// Forward declarations
namespace LLMEngineAPI {
class IConfigManager;
}

namespace LLMEngine {

/**
 * @brief Helper class for provider discovery and credential resolution.
 *
 * This class extracts provider bootstrap logic from LLMEngine, making it
 * testable in isolation and reusable across different construction paths.
 *
 * ## Responsibilities
 *
 * - Provider name resolution (default provider fallback)
 * - Provider type resolution
 * - Configuration loading
 * - Credential resolution (environment variables → constructor param → config file)
 * - Model resolution
 * - Ollama URL resolution
 *
 * ## Thread Safety
 *
 * This class is stateless and thread-safe. All methods are static.
 *
 * ## Example Usage
 *
 * ```cpp
 * auto result = ProviderBootstrap::bootstrap(
 *     "qwen",
 *     "",  // api_key (optional)
 *     "",  // model (optional)
 *     config_manager,
 *     logger.get()
 * );
 *
 * auto client = APIClientFactory::createClient(
 *     result.provider_type,
 *     result.api_key,
 *     result.model,
 *     result.ollama_url,
 *     config_manager
 * );
 * ```
 */
class LLMENGINE_EXPORT ProviderBootstrap {
  public:
    /**
     * @brief Result of provider bootstrap process.
     */
    struct BootstrapResult {
        ::LLMEngineAPI::ProviderType provider_type;
        std::string api_key;
        std::string model;
        std::string ollama_url;
    };

    /**
     * @brief Bootstrap provider configuration from provider name.
     *
     * Resolves provider name, loads configuration, and resolves credentials
     * according to priority: environment variables → constructor param → config file.
     *
     * @param provider_name Provider key (e.g., "qwen"). If empty, uses default provider.
     * @param api_key Optional API key from constructor (lowest priority).
     * @param model Optional model name. If empty, uses config default.
     * @param config_manager Configuration manager (shared ownership). If nullptr,
     *                       uses APIConfigManager::getInstance() singleton.
     * @param logger Optional logger for warnings/errors.
     * @return BootstrapResult with resolved provider configuration.
     * @throws std::runtime_error if provider not found or configuration invalid.
     */
    static BootstrapResult bootstrap(
        std::string_view provider_name,
        std::string_view api_key = "",
        std::string_view model = "",
        const std::shared_ptr<::LLMEngineAPI::IConfigManager>& config_manager = nullptr,
        Logger* logger = nullptr);

    /**
     * @brief Resolve API key with priority: env var → param → config.
     *
     * This method implements the credential resolution priority:
     * 1. Environment variable (highest priority, most secure)
     * 2. Constructor parameter (for testing/development)
     * 3. Configuration file (lowest priority, logs warning)
     *
     * @param provider_type Provider type to determine environment variable name.
     * @param api_key_from_param API key from constructor parameter.
     * @param api_key_from_config API key from configuration file.
     * @param logger Optional logger for warnings.
     * @return Resolved API key (may be empty for Ollama).
     */
    static std::string resolveApiKey(::LLMEngineAPI::ProviderType provider_type,
                                     std::string_view api_key_from_param,
                                     std::string_view api_key_from_config,
                                     Logger* logger = nullptr);

    /**
     * @brief Get environment variable name for a provider's API key.
     *
     * @param provider_type Provider type.
     * @return Environment variable name, or empty string if provider doesn't use API keys.
     */
    static std::string getApiKeyEnvVarName(::LLMEngineAPI::ProviderType provider_type);

  private:
    ProviderBootstrap() = delete; // Static class, no instances
};

} // namespace LLMEngine
