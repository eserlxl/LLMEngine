# LLMEngine Architecture

This document describes the high-level architecture of LLMEngine, including the provider interface hierarchy, configuration format, and component interactions.

## Overview

LLMEngine provides a unified, type-safe interface to multiple Large Language Model (LLM) providers. It abstracts away provider-specific details and exposes a consistent API for text generation and analysis.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  (Examples, User Code)                                      │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                     LLMEngine Class                          │
│  - High-level orchestration                                 │
│  - Analysis result formatting                               │
│  - Implements IModelContext                                 │
│  - Dependency injection for strategies                      │
└────────────┬────────────────────────────────────────────────┘
             │
             ├──────────────────┬──────────────────────────────┐
             │                  │                              │
             ▼                  ▼                              ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│RequestContext    │  │ResponseHandler   │  │DebugArtifact     │
│Builder           │  │                  │  │Manager           │
│- Uses IModelContext│ │- Handle response │  │- Write artifacts │
│- Merge params    │  │- Parse errors    │  │- Cleanup old     │
│- Build prompt    │  │- Extract content │  │- Retention       │
│- Uses IPromptBuilder│ │                  │  │                  │
└──────────────────┘  └──────────────────┘  └──────────────────┘
             │
             ├──────────────────┬──────────────────────────────┐
             │                  │                              │
             ▼                  ▼                              ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│ProviderBootstrap │  │TempDirectory     │  │IArtifactSink     │
│                  │  │Service           │  │(Strategy)        │
│- Provider        │  │- Directory       │  │- DefaultArtifact │
│  discovery       │  │  creation        │  │  Sink            │
│- Credential      │  │- Security checks │  │- Creates Debug   │
│  resolution      │  │- Path validation │  │  ArtifactManager │
│- Config loading  │  │- Permissions     │  │                  │
└──────────────────┘  └──────────────────┘  └──────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────┐
│              APIClient Interface (Abstract)                  │
│  - sendRequest(prompt, input, params)                       │
│  - getProviderType()                                        │
│  - getProviderName()                                        │
└────────────┬────────────────────────────────────────────────┘
             │
    ┌────────┴────────┬──────────┬──────────┬──────────┐
    │                 │          │          │          │
    ▼                 ▼          ▼          ▼          ▼
┌─────────┐   ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│ Qwen    │   │ OpenAI   │ │Anthropic │ │ Gemini   │ │ Ollama   │
│ Client  │   │ Client   │ │ Client   │ │ Client   │ │ Client   │
└────┬────┘   └────┬─────┘ └──────────┘ └──────────┘ └──────────┘
     │             │
     └─────┬───────┘
           │ (uses)
           ▼
┌─────────────────────────────────────────────────────────────┐
│         OpenAICompatibleClient (Helper Class)               │
│  - Shared implementation for OpenAI-compatible APIs         │
│  - Payload building                                         │
│  - URL/header caching                                       │
│  - Response parsing                                         │
└─────────────────────────────────────────────────────────────┘
           │
           ▼
┌─────────────────────────────────────────────────────────────┐
│              APIClientFactory                                │
│  - createClient(ProviderType, api_key, model, ...)          │
│  - createClientFromConfig(name, config, logger, ...)        │
│  - stringToProviderType(name)                               │
│  - Uses ProviderBootstrap for credential resolution         │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              APIConfigManager (Singleton)                    │
│  - Implements IConfigManager                                │
│  - loadConfig(path)                                         │
│  - getProviderConfig(name)                                  │
│  - getDefaultProvider()                                     │
│  - Configuration validation                                 │
│  - Thread-safe configuration access                         │
└─────────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              Dependency Injection Interfaces                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ IModelContext (LLMEngine implements)                 │  │
│  │ - Breaks cyclic dependencies                         │  │
│  │ - Provides context to RequestContextBuilder          │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ IRequestExecutor (Strategy)                          │  │
│  │ - DefaultRequestExecutor (default)                   │  │
│  │ - Injectable for testing/custom execution            │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ IPromptBuilder (Strategy)                            │  │
│  │ - TersePromptBuilder (prepends terse instruction)    │  │
│  │ - PassthroughPromptBuilder (no modification)         │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ ITempDirProvider (Strategy)                          │  │
│  │ - DefaultTempDirProvider (default)                   │  │
│  │ - Injectable for custom temp dir logic               │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ IConfigManager (Strategy)                            │  │
│  │ - APIConfigManager (default singleton)               │  │
│  │ - Injectable for custom config management            │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              Helper Components                               │
│  - ProviderBootstrap: Provider discovery & credential       │
│    resolution                                               │
│  - TempDirectoryService: Filesystem security & directory    │
│    management                                               │
│  - ParameterMerger: Merge request parameters                │
│  - ResponseParser: Parse responses with think tags          │
│  - RequestLogger: Secret redaction and logging              │
│  - Utils: Validation, file operations                       │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### LLMEngine

The main entry point for applications. It provides:
- Provider-agnostic API for analysis requests
- Request orchestration and result formatting
- Logger abstraction for observability
- Dependency injection for extensibility and testability

**Key Methods:**
- `analyze(prompt, input, analysis_type, mode, prepend_terse_instruction)` - Main analysis method
- `getProviderName()`, `getModelName()`, `getProviderType()` - Query methods
- `setLogger(logger)` - Dependency injection for logging
- `setRequestExecutor(executor)` - Inject custom request execution strategy
- `setArtifactSink(sink)` - Inject custom artifact writing strategy
- `setPromptBuilders(terse, passthrough)` - Inject custom prompt building strategies
- `setDebugFilesPolicy(policy)` - Inject policy for debug file generation

**Architecture Notes:**
- **IModelContext Implementation**: LLMEngine implements `IModelContext` to break cyclic dependencies. This allows `RequestContextBuilder` to access engine state without including the full `LLMEngine` header, improving compile times and reducing coupling.
- **Strategy Pattern**: LLMEngine uses dependency injection for several strategies (request execution, artifact writing, prompt building) to enable testing and customization.
- **Delegation**: LLMEngine delegates provider discovery and credential resolution to `ProviderBootstrap`, and filesystem operations to `TempDirectoryService`, following the Single Responsibility Principle.

### ProviderBootstrap

Helper class that extracts provider discovery and credential resolution logic:
- Provider name resolution (default provider fallback)
- Provider type resolution
- Configuration loading
- Credential resolution with priority: environment variables → constructor param → config file
- Model resolution
- Ollama URL resolution

**Key Methods:**
- `bootstrap(provider_name, api_key, model, config_manager, logger)` - Complete provider bootstrap
- `resolveApiKey(provider_type, api_key_from_param, api_key_from_config, logger)` - Credential resolution
- `getApiKeyEnvVarName(provider_type)` - Get environment variable name for a provider

### TempDirectoryService

Service class for managing temporary directories with security checks:
- Directory creation with security validation
- Symlink protection (prevents symlink traversal attacks)
- Permission setting (0700 owner-only)
- Path validation (ensures paths are within allowed root)

**Key Methods:**
- `ensureSecureDirectory(directory_path, logger)` - Create directory with security checks
- `validatePathWithinRoot(requested_path, allowed_root, logger)` - Validate path is within root
- `isDirectoryValid(directory_path, logger)` - Check if directory exists and is not a symlink

### APIClient Interface

Abstract base class defining the contract for all provider implementations:

```cpp
class APIClient {
public:
    virtual ~APIClient() = default;
    virtual APIResponse sendRequest(std::string_view prompt, 
                                   const nlohmann::json& input,
                                   const nlohmann::json& params) const = 0;
    virtual ProviderType getProviderType() const = 0;
    virtual std::string getProviderName() const = 0;
};
```

### Provider Implementations

Each provider implements the `APIClient` interface with provider-specific logic:

1. **QwenClient** - DashScope API integration
2. **OpenAIClient** - OpenAI API integration
3. **AnthropicClient** - Anthropic Claude API integration
4. **GeminiClient** - Google Gemini API integration
5. **OllamaClient** - Local Ollama server integration

### APIClientFactory

Factory pattern for creating provider instances:

```cpp
std::unique_ptr<APIClient> createClient(
    ProviderType type,
    const std::string& api_key,
    const std::string& model,
    const std::string& base_url = ""
);
```

### APIConfigManager

Singleton managing configuration loading and access:
- Implements `IConfigManager` interface for dependency injection
- Thread-safe configuration access using reader-writer locks
- Environment variable preference for API keys
- Default provider resolution
- Configuration file parsing (`config/api_config.json`)

## Dependency Injection and Strategy Patterns

LLMEngine uses extensive dependency injection to enable testability, flexibility, and separation of concerns. The following interfaces define the injection points:

### IModelContext

Interface implemented by `LLMEngine` to break cyclic dependencies and provide context to helper classes:

```cpp
class IModelContext {
public:
    virtual std::string getTempDirectory() const = 0;
    virtual std::shared_ptr<IPromptBuilder> getTersePromptBuilder() const = 0;
    virtual std::shared_ptr<IPromptBuilder> getPassthroughPromptBuilder() const = 0;
    virtual const nlohmann::json& getModelParams() const = 0;
    virtual bool areDebugFilesEnabled() const = 0;
    virtual std::shared_ptr<IArtifactSink> getArtifactSink() const = 0;
    virtual int getLogRetentionHours() const = 0;
    virtual std::shared_ptr<Logger> getLogger() const = 0;
    virtual void prepareTempDirectory() const = 0;
};
```

**Purpose**: Allows `RequestContextBuilder` and other helpers to access engine state without creating a circular dependency. This improves compile times and reduces coupling.

**Thread Safety**: Implementations should follow the same thread-safety guarantees as `LLMEngine` (typically not thread-safe for modifications, but safe for read-only access).

### IRequestExecutor

Strategy interface for executing provider requests:

```cpp
class IRequestExecutor {
public:
    virtual APIResponse execute(
        const APIClient* api_client,
        const std::string& full_prompt,
        const nlohmann::json& input,
        const nlohmann::json& final_params) const = 0;
};
```

**Default Implementation**: `DefaultRequestExecutor` directly calls `APIClient::sendRequest()`.

**Use Cases**:
- Testing: Mock request execution for unit tests
- Custom execution: Add retry logic, rate limiting, or request transformation
- Observability: Log or trace requests before execution

**Injection**: Use `LLMEngine::setRequestExecutor()` to inject a custom executor.

### IArtifactSink

Strategy interface for debug artifact creation and writing:

```cpp
class IArtifactSink {
public:
    virtual std::unique_ptr<DebugArtifactManager> create(
        const std::string& request_tmp_dir,
        const std::string& base_tmp_dir,
        int log_retention_hours,
        Logger* logger) const = 0;
    
    virtual void writeApiResponse(
        DebugArtifactManager* mgr,
        const APIResponse& response,
        bool is_error) const = 0;
};
```

**Default Implementation**: `DefaultArtifactSink` creates and uses `DebugArtifactManager` for file-based artifact storage.

**Use Cases**:
- Testing: No-op sink to disable artifact writing in tests
- Custom storage: Write artifacts to databases, cloud storage, or custom formats
- Filtering: Selective artifact writing based on request characteristics

**Injection**: Use `LLMEngine::setArtifactSink()` to inject a custom sink.

### IPromptBuilder

Strategy interface for building prompts from user input:

```cpp
class IPromptBuilder {
public:
    virtual std::string buildPrompt(std::string_view prompt) const = 0;
};
```

**Implementations**:
- **TersePromptBuilder** (default): Prepends a system instruction asking for brief, concise responses (one to two sentences). Used when `prepend_terse_instruction=true`.
- **PassthroughPromptBuilder**: Returns the prompt unchanged. Used when `prepend_terse_instruction=false`.

**Use Cases**:
- Custom prompt formatting: Add prefixes, suffixes, or structured formatting
- Multi-turn conversations: Build prompts with conversation history
- Evaluation: Use passthrough builder for exact prompt matching

**Injection**: Use `LLMEngine::setPromptBuilders()` to inject custom builders for both terse and passthrough modes.

### ITempDirProvider

Interface for providing temporary directory paths:

```cpp
class ITempDirProvider {
public:
    virtual std::string getTempDir() const = 0;
};
```

**Default Implementation**: `DefaultTempDirProvider` returns a platform-appropriate temporary directory (e.g., `/tmp/llmengine` on Unix).

**Thread Safety**: Implementations **MUST** be thread-safe, as `getTempDir()` may be called concurrently from multiple threads.

**Use Cases**:
- Testing: Mock temp directories for isolated test execution
- Multi-tenant systems: Per-tenant directory isolation
- Custom cleanup: Implement custom retention or cleanup policies

**Injection**: Pass a `shared_ptr<ITempDirProvider>` to the LLMEngine constructor.

### IConfigManager

Interface for configuration management:

```cpp
class IConfigManager {
public:
    virtual void setDefaultConfigPath(std::string_view config_path) = 0;
    virtual std::string getDefaultConfigPath() const = 0;
    virtual void setLogger(std::shared_ptr<Logger> logger) = 0;
    virtual bool loadConfig(std::string_view config_path = "") = 0;
    virtual nlohmann::json getProviderConfig(std::string_view provider_name) const = 0;
    virtual std::vector<std::string> getAvailableProviders() const = 0;
    virtual std::string getDefaultProvider() const = 0;
    virtual int getTimeoutSeconds() const = 0;
    virtual int getTimeoutSeconds(std::string_view provider_name) const = 0;
    virtual int getRetryAttempts() const = 0;
    virtual int getRetryDelayMs() const = 0;
};
```

**Default Implementation**: `APIConfigManager` provides a singleton-based configuration system with thread-safe access.

**Thread Safety**: All methods **MUST** be thread-safe and can be called concurrently from multiple threads.

**Use Cases**:
- Testing: Mock configuration for unit tests
- Dynamic configuration: Load configuration from databases or remote services
- Multi-tenant: Per-tenant configuration isolation

**Injection**: Pass a `shared_ptr<IConfigManager>` to the LLMEngine constructor (defaults to `APIConfigManager::getInstance()` singleton).

## Dependency Injection Usage

### Example: Custom Request Executor

```cpp
class LoggingRequestExecutor : public IRequestExecutor {
public:
    APIResponse execute(const APIClient* api_client,
                       const std::string& full_prompt,
                       const nlohmann::json& input,
                       const nlohmann::json& final_params) const override {
        // Log request before execution
        std::cout << "Executing request with prompt: " << full_prompt << std::endl;
        
        // Delegate to default executor
        DefaultRequestExecutor default_executor;
        auto response = default_executor.execute(api_client, full_prompt, input, final_params);
        
        // Log response
        std::cout << "Response success: " << response.success << std::endl;
        return response;
    }
};

// Usage
LLMEngine engine(ProviderType::QWEN, api_key, "qwen-flash");
engine.setRequestExecutor(std::make_shared<LoggingRequestExecutor>());
```

### Example: Custom Prompt Builder

```cpp
class CustomPromptBuilder : public IPromptBuilder {
public:
    std::string buildPrompt(std::string_view prompt) const override {
        return "[SYSTEM] You are a helpful assistant.\n[USER] " + std::string(prompt);
    }
};

// Usage
LLMEngine engine(ProviderType::QWEN, api_key, "qwen-flash");
engine.setPromptBuilders(
    std::make_shared<CustomPromptBuilder>(),  // Terse builder
    std::make_shared<PassthroughPromptBuilder>()  // Passthrough builder
);
```

### Example: Testing with Mock Components

```cpp
class MockArtifactSink : public IArtifactSink {
public:
    std::unique_ptr<DebugArtifactManager> create(...) const override {
        return nullptr;  // No-op for testing
    }
    void writeApiResponse(...) const override {
        // No-op for testing
    }
};

// Usage in tests
LLMEngine engine(ProviderType::QWEN, api_key, "qwen-flash");
engine.setArtifactSink(std::make_shared<MockArtifactSink>());
```

## Configuration Format

The configuration file (`config/api_config.json`) follows this structure:

```json
{
  "providers": {
    "qwen": {
      "base_url": "https://dashscope-intl.aliyuncs.com/compatible-mode/v1",
      "default_model": "qwen-flash",
      "default_params": {
        "temperature": 0.7,
        "max_tokens": 2000
      }
    },
    "openai": {
      "base_url": "https://api.openai.com/v1",
      "default_model": "gpt-3.5-turbo",
      "default_params": {
        "temperature": 0.7,
        "max_tokens": 2000
      }
    }
  },
  "default_provider": "qwen",
  "timeout_seconds": 30,
  "retry_attempts": 3
}
```

### Configuration Priority

1. **Environment Variables** (highest priority)
   - `QWEN_API_KEY`, `OPENAI_API_KEY`, `ANTHROPIC_API_KEY`, `GEMINI_API_KEY`
2. **Constructor Parameters**
   - API keys and model names passed directly
3. **Configuration File** (lowest priority)
   - Fallback for API keys (with security warning)
   - Default models and parameters

## Request Flow

### Sequence Diagram: Successful Request

```
Application
    │
    │ analyze(prompt, input, analysis_type, ...)
    ▼
LLMEngine
    │
    │ 1. Build request context
    │    RequestContextBuilder::build()
    │    ├─ Merge parameters (ParameterMerger)
    │    ├─ Build full prompt
    │    └─ Create DebugArtifactManager
    ▼
    │
    │ 2. Send request
    │    api_client_->sendRequest(full_prompt, input, params)
    ▼
APIClient (e.g., QwenClient/OpenAIClient)
    │
    │ 3. Prepare request (OpenAICompatibleClient)
    │    ├─ Build payload (ChatCompletionRequestHelper)
    │    ├─ Get cached URL/headers
    │    └─ Serialize JSON
    ▼
    │
    │ 4. HTTP request (CPR library)
    │    ├─ Retry logic with exponential backoff
    │    └─ Timeout handling
    ▼
    │
    │ 5. Parse response
    │    OpenAICompatibleClient::parseOpenAIResponse()
    │    └─ Extract content from JSON
    ▼
    │
    │ 6. Return APIResponse
    ▼
LLMEngine
    │
    │ 7. Handle response
    │    ResponseHandler::handle()
    │    ├─ Check success/error
    │    ├─ Write debug artifacts (if enabled)
    │    └─ Parse think tags (ResponseParser)
    ▼
    │
    │ 8. Return AnalysisResult
    ▼
Application
```

### Sequence Diagram: Error Handling Flow

```
Application
    │
    │ analyze(...)
    ▼
LLMEngine
    │
    │ Request fails (network/timeout/API error)
    ▼
APIClient
    │
    │ Returns APIResponse with success=false
    │ ├─ error_code: LLMEngineErrorCode
    │ ├─ error_message: string
    │ └─ status_code: int
    ▼
LLMEngine
    │
    │ ResponseHandler::handle()
    │ ├─ Classify error code
    │ │  ├─ HTTP 401/403 → Auth error
    │  │  ├─ HTTP 429 → RateLimited
    │  │  ├─ HTTP 4xx → Client error
    │  │  └─ HTTP 5xx → Server error
    │ ├─ Enhance error message with HTTP status
    │ ├─ Redact secrets (RequestLogger::logSafe)
    │ └─ Write error artifact (if debug enabled)
    ▼
    │
    │ Return AnalysisResult with success=false
    ▼
Application
    │
    │ Check result.success and handle error
    ▼
```

### Sequence Diagram: Configuration Loading and Provider Bootstrap

```
Application
    │
    │ LLMEngine constructor with provider_name
    ▼
ProviderBootstrap::bootstrap()
    │
    │ 1. Resolve config manager (injected or singleton)
    ▼
APIConfigManager (Singleton)
    │
    │ 2. Load JSON file
    │    ├─ Parse config/api_config.json
    │    └─ Handle parse errors
    ▼
    │
    │ 3. Validate configuration
    │    ├─ Validate JSON structure
    │    ├─ Validate provider configs
    │    │  ├─ base_url format (Utils::validateUrl)
    │    │  └─ model name format (Utils::validateModelName)
    │    └─ Validate global settings
    │       ├─ timeout_seconds range
    │       ├─ retry_attempts range
    │       └─ retry_delay_ms range
    ▼
ProviderBootstrap
    │
    │ 4. Resolve provider name (default if empty)
    │ 5. Get provider configuration
    │ 6. Resolve provider type
    │ 7. Resolve credentials (env var → param → config)
    │ 8. Resolve model name
    │ 9. Resolve Ollama URL (if applicable)
    ▼
    │
    │ Return BootstrapResult
    ▼
LLMEngine
    │
    │ Store resolved values and create APIClient
    ▼
Application
```

### Detailed Request Flow Steps

1. **Application calls `LLMEngine::analyze()`**
   - Prompt is optionally prepended with terse instruction
   - Input parameters are merged with model defaults via `ParameterMerger`

2. **Request context building** (`RequestContextBuilder`)
   - Uses `IModelContext` interface to access engine state (avoids cyclic dependency)
   - Selects prompt builder based on `prepend_terse_instruction` flag:
     - `TersePromptBuilder` if `prepend_terse_instruction=true` (default)
     - `PassthroughPromptBuilder` if `prepend_terse_instruction=false`
   - Builds full prompt using selected builder
   - Merges input parameters with defaults via `ParameterMerger`
   - Creates temporary directory for debug artifacts using `ITempDirProvider`
   - Initializes `DebugArtifactManager` via `IArtifactSink` if debug files enabled

3. **LLMEngine delegates to APIClient via IRequestExecutor**
   - Uses injected `IRequestExecutor` strategy (default: `DefaultRequestExecutor`)
   - `request_executor_->execute(api_client_, full_prompt, input, api_params)`
   - Default executor directly calls `api_client_->sendRequest()`

4. **Provider-specific request preparation**
   - For OpenAI-compatible providers: Uses `OpenAICompatibleClient` helper
     - Message formatting via `ChatCompletionRequestHelper`
     - Parameter merging
     - Cached URL and header construction
   - For other providers: Provider-specific implementation

5. **HTTP request with retry logic**
   - Exponential backoff with jitter
   - Configurable retry attempts
   - Timeout handling

6. **Response processing**
   - Status code checking
   - JSON parsing
   - Error extraction
   - Content extraction (provider-specific)

7. **Response handling** (`ResponseHandler`)
   - Error classification based on HTTP status and error codes
   - Error message enhancement with context
   - Secret redaction for logging
   - Debug artifact generation via `IArtifactSink` (if enabled)
     - Uses injected artifact sink to write artifacts
     - Default sink uses `DebugArtifactManager` for file-based storage

8. **Debug artifact generation** (if enabled)
   - Redacted JSON responses
   - Full text responses
   - Retention policy enforcement

9. **Result formatting**
   - THINK section extraction (via `ResponseParser`)
   - Content section extraction
   - `AnalysisResult` construction

## Temporary Artifact Management

### Directory Structure

All debug artifacts are stored in `Utils::TMP_DIR` (default: `/tmp/llmengine`). Each analysis request creates a unique subdirectory to avoid conflicts in concurrent scenarios:

```
/tmp/llmengine/
├── req_{timestamp}_{thread_id}/
│   ├── api_response.json          # Redacted API response
│   ├── api_response_error.json    # Redacted error response (on error)
│   ├── response_full.txt          # Full response text
│   ├── {analysis_type}.think.txt  # Extracted thinking section
│   └── {analysis_type}.txt        # Remaining content
└── ...
```

The unique directory name format is `req_{milliseconds}_{thread_id}`, ensuring that concurrent requests from the same or different threads do not overwrite each other's artifacts.

### Security

- **Directory Permissions**: 0700 (owner-only access)
- **Symlink Protection**: Temporary directories cannot be symlinks; the library throws an exception if a symlink is detected to prevent symlink traversal attacks
- **Secret Redaction**: API keys and sensitive tokens are automatically redacted
- **Retention Policy**: Files older than `log_retention_hours` are automatically cleaned up (cleanup scans the base temp directory)
- **Environment Control**: Set `LLMENGINE_DISABLE_DEBUG_FILES=1` to disable file generation. Alternatively, inject a policy via `setDebugFilesPolicy(std::function<bool()>)` to toggle behavior dynamically per-request or from application settings.
- **Concurrent Safety**: Each request uses a unique subdirectory to prevent race conditions and file conflicts

### Retention Behavior

- Old files are cleaned up based on `log_retention_hours` parameter
- Cleanup runs automatically after each debug file write
- Uses `DebugArtifacts::cleanupOld()` for retention enforcement

## Logger Abstraction

The library uses a pluggable logger interface:

```cpp
struct Logger {
    virtual ~Logger() = default;
    virtual void log(LogLevel level, std::string_view message) = 0;
};
```

**Log Levels:**
- `Debug` - Debug information (debug file operations)
- `Info` - General information
- `Warn` - Warnings (config fallbacks, permission errors)
- `Error` - Errors (API failures, parsing errors)

**Default Implementation:**
- `DefaultLogger` writes to `std::cout` (Debug/Info) and `std::cerr` (Warn/Error)

**Custom Loggers:**
- Applications can inject custom loggers via `setLogger()`
- Useful for integration with logging frameworks (spdlog, log4cpp, etc.)

## Error Handling

### API Response Errors

- HTTP status codes are preserved in `AnalysisResult.statusCode`
- Error messages are extracted from provider responses
- Retry logic handles transient failures

### Configuration Errors

- Missing provider configurations throw `std::runtime_error`
- Invalid provider names fail fast (no silent fallback)
- Configuration file parsing errors are logged and handled gracefully

### Filesystem Errors

- Permission errors use `std::error_code` to avoid exceptions
- Directory creation failures are logged but don't abort operations
- File write failures are silently ignored (best-effort)

## Thread Safety

- **APIConfigManager**: Thread-safe (reader-writer locks)
- **LLMEngine**: Not thread-safe (intended for single-threaded use per instance)
- **APIClient implementations**: Stateless and thread-safe
- **Logger**: Must be thread-safe if shared across threads

## Extension Points

### Adding New Providers

1. Create a new client class inheriting from `APIClient`
2. Implement `sendRequest()`, `getProviderType()`, `getProviderName()`
3. Add `ProviderType` enum value
4. Update `APIClientFactory::createClient()` to handle new type
5. Update `APIClientFactory::stringToProviderType()` for string mapping

## Design Principles

1. **Provider Abstraction**: All providers implement the same `APIClient` interface, allowing seamless switching between providers.
2. **Configuration-Driven**: Provider settings are loaded from JSON configuration files, with environment variable overrides.
3. **Thread Safety**: APIClient implementations are stateless and thread-safe, while LLMEngine instances are not thread-safe.
4. **Error Handling**: Comprehensive error classification and retry logic for transient failures.
5. **Security**: API keys are never logged, and environment variables are preferred over config files.
6. **Extensibility**: New providers can be added by implementing the `APIClient` interface.
7. **Single Responsibility Principle (SRP)**: Components are separated by responsibility:
   - `LLMEngine`: Request orchestration and result formatting
   - `ProviderBootstrap`: Provider discovery and credential resolution
   - `TempDirectoryService`: Filesystem security and directory management
   - `RequestContextBuilder`: Request context construction
   - `ResponseHandler`: Response processing and error handling
8. **Dependency Injection**: Strategy patterns enable testability and extensibility:
   - `IRequestExecutor`: Customize request execution
   - `IArtifactSink`: Customize artifact storage
   - `IPromptBuilder`: Customize prompt formatting
   - `ITempDirProvider`: Customize temporary directory management
   - `IConfigManager`: Customize configuration management
9. **Interface Segregation**: `IModelContext` interface breaks cyclic dependencies by providing minimal context to helper classes without requiring full `LLMEngine` header inclusion.
10. **Testability**: Extracted services and dependency injection enable independent testing:
    - Services (`ProviderBootstrap`, `TempDirectoryService`) can be tested independently
    - Strategies can be mocked for unit testing
    - `LLMEngine` can be tested with mock dependencies

#### Design Rationale: Interface vs Concepts

LLMEngine uses a traditional abstract base class (`APIClient`) rather than C++20 concepts for provider abstraction:

**Benefits of current approach:**
- Runtime polymorphism: Enables dynamic provider selection via factory pattern
- Type erasure: `LLMEngine` can hold providers via `unique_ptr<APIClient>` without templates
- Backward compatibility: Works with C++20 compilers without requiring concepts support
- Clear ownership semantics: Virtual destructor ensures proper cleanup

**Alternative (concepts) consideration:**
- C++20 concepts would provide compile-time polymorphism and better type safety
- However, concepts require template-based design, which would complicate the factory pattern
- Current approach provides better runtime flexibility for configuration-driven provider selection

**Future enhancement:**
If compile-time polymorphism becomes a priority, we could add concept-based provider traits alongside the existing interface, allowing both approaches to coexist.

6. Add configuration entry in `config/api_config.json`

### Custom Logging

Implement the `Logger` interface and inject via `setLogger()`:

```cpp
class MyLogger : public Logger {
    void log(LogLevel level, std::string_view message) override {
        // Custom logging implementation
    }
};

engine.setLogger(std::make_shared<MyLogger>());
```

### Custom Strategies

See the [Dependency Injection and Strategy Patterns](#dependency-injection-and-strategy-patterns) section for examples of:
- Custom request executors
- Custom artifact sinks
- Custom prompt builders
- Custom temp directory providers
- Custom configuration managers

## Performance Considerations

- **Temporary Directory Caching**: Directory setup is cached to avoid repeated filesystem operations
- **Precompiled Regexes**: Regular expressions are precompiled to reduce overhead
- **RAII Resource Management**: File descriptors are managed with RAII to prevent leaks
- **JSON Payload Optimization**: Consider using `merge_patch` or in-place mutation for high-frequency requests

## Security Considerations

- **API Key Management**: Environment variables are preferred over config files
- **Secret Redaction**: All debug artifacts are automatically redacted
- **Directory Isolation**: Temporary directories use restrictive permissions (0700)
- **Command Execution**: `Utils::execCommand()` uses `posix_spawn()` to avoid shell injection
- **Input Validation**: Command strings are validated to prevent injection attacks

## See Also

- [docs/configuration.md](configuration.md) - Configuration format and structure
- [docs/api_reference.md](api_reference.md) - API usage and examples
- [docs/security.md](security.md) - Security considerations
- [docs/performance.md](performance.md) - Performance considerations
- [quickstart.md](../quickstart.md) - Quick start guide
- [README.md](../README.md) - Main documentation

