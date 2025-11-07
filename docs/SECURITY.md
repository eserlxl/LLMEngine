# Security

This document outlines how LLMEngine handles sensitive data, configuration, and safe execution.

## API Key Management

- Always use environment variables for API keys. The library automatically checks:
  - `QWEN_API_KEY`
  - `OPENAI_API_KEY`
  - `ANTHROPIC_API_KEY`
  - `GEMINI_API_KEY`

```bash
export QWEN_API_KEY="sk-your-qwen-key"
export OPENAI_API_KEY="sk-your-openai-key"
```

- Never commit API keys to version control. `config/api_config.json` must only contain non-secret defaults.
- Environment variables take precedence over config file values.
- **Fail-fast behavior**: If no API key is found for a provider that requires one (all except Ollama), the library throws a `std::runtime_error` instead of proceeding silently. This prevents silent failures in headless or hardened deployments.
- If a key is found in `api_config.json` but not in environment variables, a warning is emitted to stderr. For production, ensure keys are provided via environment variables to avoid this fallback.

### API Key Lifetime and Memory Management

**Current Implementation:**
- API keys are stored as `std::string` in `LLMEngine` instances for the lifetime of the engine.
- Keys are resolved once at construction time and remain in memory until the engine is destroyed.
- Keys are passed to API clients via `std::string_view` to avoid unnecessary copies during request execution.

**Security Considerations:**
- API keys persist in memory for the entire lifetime of the `LLMEngine` instance.
- Memory may be swapped to disk by the operating system, potentially exposing keys in swap files.
- For enhanced security, consider using `SecureString` (see below) for temporary key storage, or minimize the lifetime of `LLMEngine` instances that hold sensitive keys.

**SecureString Wrapper (Optional):**
The library provides a `SecureString` class (`LLMEngine/SecureString.hpp`) that automatically scrubs memory on destruction:

```cpp
#include "LLMEngine/SecureString.hpp"

// Store API key in SecureString for temporary use
SecureString api_key("sk-your-key");
// Use api_key.view() or api_key.c_str() as needed
// Memory is automatically scrubbed when api_key goes out of scope
```

**Note:** `SecureString` is a defense-in-depth measure and does not guarantee complete security. The compiler may optimize away scrubbing, or memory may be copied elsewhere. For maximum security, prefer environment variables and minimize the lifetime of objects holding secrets.

## Debug Mode and Sensitive Data

When debug mode is enabled (`debug=true`), LLMEngine writes response artifacts to `/tmp/llmengine`.

- **Unique directories per request**: Each analysis request creates a unique timestamped subdirectory (e.g., `req_{milliseconds}_{thread_id}/`) to prevent concurrent requests from overwriting each other's artifacts.
- Debug files are automatically sanitized: API keys and other sensitive fields are redacted before writing to disk.
- **Symlink protection**: The library throws an exception if the temporary directory is a symlink to prevent symlink traversal attacks.
- Log retention is enforced: files older than `log_retention_hours` are automatically cleaned up.
- Disable debug files in production with `LLMENGINE_DISABLE_DEBUG_FILES=1`.
- Debug artifact directories are created with 0700 permissions to prevent cross-user access on shared hosts.

```cpp
// Debug mode with 24-hour retention
LLMEngine engine(provider_type, api_key, model, params, 24, true);

// Disable debug files entirely
setenv("LLMENGINE_DISABLE_DEBUG_FILES", "1", 1);
```

#### Debug Files Environment Variable Caching Strategy

LLMEngine caches the `LLMENGINE_DISABLE_DEBUG_FILES` environment variable value at construction time:

**Caching Behavior:**
- The environment variable is read once at `LLMEngine` construction time and cached in `disable_debug_files_env_cached_`
- Per-request checks (`areDebugFilesEnabled()`) use the cached value if no policy is injected
- This avoids repeated `std::getenv()` calls on hot paths

**Runtime Control:**
- **Policy injection**: Use `setDebugFilesPolicy()` to inject a custom policy function for dynamic control
- **Direct setter**: Use `setDebugFilesEnabled(bool)` for simple runtime toggling
- **Read-once semantics**: Changes to the environment variable after construction are not reflected unless a policy is used

**Recommendations:**
- **For tests**: Use `setDebugFilesEnabled(false)` or `setDebugFilesPolicy()` to control behavior without environment variable dependencies
- **For long-lived services**: Use `setDebugFilesPolicy()` with a configuration-based policy for runtime control
- **For runtime toggling**: Use `setDebugFilesEnabled()` or inject a policy that checks a configuration source

**Example:**
```cpp
// Simple runtime toggle
engine.setDebugFilesEnabled(false);

// Long-lived service: inject policy for dynamic control
auto debug_policy = []() { return std::getenv("LLMENGINE_DISABLE_DEBUG_FILES") == nullptr; };
engine.setDebugFilesPolicy(debug_policy);

// Or use a configuration-based policy
auto config_policy = [&config]() { return config->shouldWriteDebugFiles(); };
engine.setDebugFilesPolicy(config_policy);
```

## Command Execution Safety

`Utils::execCommand()` uses `posix_spawn()` and does not route through a shell, eliminating shell injection risks:

- **Platform-specific**: This function is only available on POSIX systems (Linux, macOS). On Windows, it returns an empty vector and logs an error.
- No shell involvement: commands are executed via `posix_spawn()` with explicit argv vectors.
- Control characters are rejected: newlines, tabs, carriage returns, and all control characters are blocked.
- Shell metacharacters are rejected: `|`, `&`, `;`, `$`, `` ` ``, `<`, `>`, parentheses, brackets, and wildcards are blocked.
- Whitelist approach: only alphanumeric, single spaces, hyphens, underscores, dots, and slashes are allowed.
- Multiple consecutive spaces are rejected to prevent obfuscation.
- **Output size limits**: To prevent memory exhaustion from user-controlled commands, output is limited to 10,000 lines with a maximum line length of 1MB per line.

Security benefits:
- Bypasses shell interpretation entirely.
- Validation is enforced as defense-in-depth.
- Resource limits protect against denial-of-service attacks.
- Only trusted commands should be passed to this function.

## Provider Selection Safety

- Unknown providers fail fast: invalid provider names throw exceptions instead of silently falling back, preventing unexpected routing.

## Thread Safety

- `APIConfigManager` is thread-safe via reader-writer locks. Concurrent reads and single-writer updates are safe.

## Best Practices

- Do not hardcode API keys; always use environment variables.
- Prefer secure endpoints (HTTPS) and verify firewall/network settings.
- Keep secrets out of version control and review debug artifacts before sharing.

Example:

```cpp
// Correct
const char* api_key = std::getenv("QWEN_API_KEY");
```

## See Also

- [docs/CONFIGURATION.md](CONFIGURATION.md) - Configuration and API key setup
- [docs/ARCHITECTURE.md](ARCHITECTURE.md) - Security considerations in architecture
- [docs/FAQ.md](FAQ.md) - Common security questions
- [QUICKSTART.md](../QUICKSTART.md) - Security best practices in quick start
- [README.md](../README.md) - Main documentation

