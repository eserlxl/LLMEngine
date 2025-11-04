# ADR-001: Architecture Boundaries and Responsibilities

**Status:** Accepted  
**Date:** 2025-01-XX  
**Decision Makers:** Development Team

## Context

LLMEngine is a C++ library that provides a unified interface for interacting with multiple LLM providers. As the library grows, it's important to establish clear boundaries between components to ensure maintainability, testability, and extensibility.

## Decision

We establish the following architectural boundaries and responsibilities:

### 1. Core Components

#### LLMEngine (Orchestration Layer)
- **Responsibility:** High-level orchestration of analysis requests
- **Boundaries:**
  - Owns APIClient instances (via unique_ptr)
  - Delegates provider-specific logic to APIClient implementations
  - Manages debug artifact generation through DebugArtifacts
  - Handles prompt preprocessing and response post-processing
  - Owns logger instance (via shared_ptr)
- **Dependencies:** APIClient, Logger, DebugArtifacts, ITempDirProvider
- **Thread Safety:** Not thread-safe (intended for single-threaded use per instance)

#### APIClient (Provider Abstraction Layer)
- **Responsibility:** Provider-specific API communication
- **Boundaries:**
  - Stateless request/response handling
  - Provider-specific request formatting
  - HTTP retry logic and error handling
  - Response parsing and normalization
- **Dependencies:** cpr (HTTP), nlohmann_json (JSON)
- **Thread Safety:** Thread-safe (stateless implementations)

#### APIClientFactory (Factory Pattern)
- **Responsibility:** Creation and configuration of APIClient instances
- **Boundaries:**
  - Provider type resolution
  - Credential resolution (environment variables > config file)
  - Client instantiation
- **Dependencies:** APIClient implementations, APIConfigManager
- **Thread Safety:** Thread-safe (stateless factory methods)

#### APIConfigManager (Configuration Singleton)
- **Responsibility:** Configuration file loading and access
- **Boundaries:**
  - JSON configuration parsing
  - Thread-safe configuration access
  - Provider configuration retrieval
- **Dependencies:** nlohmann_json, Logger
- **Thread Safety:** Thread-safe (reader-writer locks)

### 2. Dependency Injection Points

#### Logger Interface
- **Purpose:** Pluggable logging abstraction
- **Default:** DefaultLogger (thread-safe console output)
- **Injection:** Via `LLMEngine::setLogger()` or constructor parameters
- **Thread Safety:** Must be thread-safe if shared across threads

#### ITempDirProvider Interface
- **Purpose:** Temporary directory path provider
- **Default:** DefaultTempDirProvider (returns "/tmp/llmengine")
- **Injection:** Via constructor parameter (dependency injection constructor)
- **Thread Safety:** Must be thread-safe

### 3. Design Boundaries

#### Provider Implementations
- **Separation:** Each provider (QwenClient, OpenAIClient, etc.) is in its own .cpp file
- **Shared Code:** Common retry logic and helpers in APIClientCommon.hpp
- **Independence:** Provider implementations should not depend on each other

#### Configuration Management
- **Singleton Pattern:** APIConfigManager uses singleton for global configuration access
- **Thread Safety:** All configuration access is protected by reader-writer locks
- **Dependency Injection:** Future: APIConfigManager should be injectable via interface

#### Debug Artifacts
- **Responsibility:** DebugArtifacts handles file I/O operations
- **Security:** Automatic secret redaction, secure directory permissions
- **Lifecycle:** Per-request directories prevent race conditions

### 4. Extension Points

#### Adding New Providers
1. Create new client class inheriting from APIClient
2. Implement sendRequest(), getProviderType(), getProviderName()
3. Add ProviderType enum value
4. Update APIClientFactory::createClient()
5. Update APIConfigManager string mapping

#### Custom Logging
- Implement Logger interface
- Inject via LLMEngine::setLogger()

#### Custom Temp Directory Provider
- Implement ITempDirProvider interface
- Inject via dependency injection constructor

## Consequences

### Positive
- Clear separation of concerns
- Improved testability through dependency injection
- Better thread-safety guarantees
- Easier to add new providers
- Reduced coupling between components

### Negative
- Slightly more complex constructor signatures
- Need to maintain interface contracts
- More files to manage (but better modularity)

## Alternatives Considered

1. **Global Configuration:** Rejected - not thread-safe, hard to test
2. **Tight Coupling:** Rejected - would make testing and extension difficult
3. **No Interfaces:** Rejected - would prevent dependency injection and testing

## Notes

- Utils::TMP_DIR is deprecated but kept for backward compatibility
- APIConfigManager singleton pattern may be replaced with dependency injection in future versions
- Prompt building and debug artifact management may be extracted into separate classes for better modularity

