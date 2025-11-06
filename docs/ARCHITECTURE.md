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
│  - Debug artifact management                                │
│  - Logger injection                                         │
│  - Request context building                                 │
└────────────┬────────────────────────────────────────────────┘
             │
             ├──────────────────┬──────────────────────────────┐
             │                  │                              │
             ▼                  ▼                              ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│RequestContext    │  │ResponseHandler   │  │DebugArtifact     │
│Builder           │  │                  │  │Manager           │
│- Build context   │  │- Handle response │  │- Write artifacts │
│- Merge params    │  │- Parse errors    │  │- Cleanup old     │
│- Build prompt    │  │- Extract content │  │- Retention       │
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
│  - stringToProviderType(name)                               │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              APIConfigManager (Singleton)                    │
│  - loadConfig(path)                                         │
│  - getProviderConfig(name)                                  │
│  - getDefaultProvider()                                     │
│  - Configuration validation                                 │
│  - Thread-safe configuration access                         │
└─────────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              Helper Components                               │
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
- Configuration management
- Debug artifact generation and cleanup
- Logger abstraction for observability

**Key Methods:**
- `analyze(prompt, input, analysis_type, mode, prepend_terse_instruction)` - Main analysis method
- `getProviderName()`, `getModelName()`, `getProviderType()` - Query methods
- `setLogger(logger)` - Dependency injection for logging

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
- Thread-safe configuration access using reader-writer locks
- Environment variable preference for API keys
- Default provider resolution
- Configuration file parsing (`config/api_config.json`)

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

### Sequence Diagram: Configuration Loading

```
Application
    │
    │ LLMEngine constructor or
    │ APIConfigManager::loadConfig()
    ▼
APIConfigManager (Singleton)
    │
    │ 1. Load JSON file
    │    ├─ Parse config/api_config.json
    │    └─ Handle parse errors
    ▼
    │
    │ 2. Validate configuration
    │    ├─ Validate JSON structure
    │    ├─ Validate provider configs
    │    │  ├─ base_url format (Utils::validateUrl)
    │    │  └─ model name format (Utils::validateModelName)
    │    └─ Validate global settings
    │       ├─ timeout_seconds range
    │       ├─ retry_attempts range
    │       └─ retry_delay_ms range
    ▼
    │
    │ 3. Store configuration
    │    ├─ Thread-safe write lock
    │    └─ Update internal state
    ▼
    │
    │ 4. Return success/failure
    ▼
Application
```

### Detailed Request Flow Steps

1. **Application calls `LLMEngine::analyze()`**
   - Prompt is optionally prepended with terse instruction
   - Input parameters are merged with model defaults via `ParameterMerger`

2. **Request context building** (`RequestContextBuilder`)
   - Builds full prompt with system/user messages
   - Merges input parameters with defaults
   - Creates temporary directory for debug artifacts
   - Initializes `DebugArtifactManager` if debug files enabled

3. **LLMEngine delegates to APIClient**
   - `api_client_->sendRequest(full_prompt, input, api_params)`

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
   - Debug artifact generation (if enabled)

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

- [docs/CONFIGURATION.md](CONFIGURATION.md) - Configuration format and structure
- [docs/API_REFERENCE.md](API_REFERENCE.md) - API usage and examples
- [docs/SECURITY.md](SECURITY.md) - Security considerations
- [docs/PERFORMANCE.md](PERFORMANCE.md) - Performance considerations
- [QUICKSTART.md](../QUICKSTART.md) - Quick start guide
- [README.md](../README.md) - Main documentation
