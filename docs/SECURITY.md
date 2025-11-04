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
- If a key is not found in env or constructor parameters and the library falls back to `api_config.json`, a warning is emitted to stderr. For production, ensure keys are provided via environment variables to avoid this fallback.

## Debug Mode and Sensitive Data

When debug mode is enabled (`debug=true`), LLMEngine writes response artifacts to `/tmp/llmengine`.

- Debug files are automatically sanitized: API keys and other sensitive fields are redacted before writing to disk.
- Log retention is enforced: files older than `log_retention_hours` are automatically cleaned up.
- Disable debug files in production with `LLMENGINE_DISABLE_DEBUG_FILES=1`.
- Debug artifact directories are created with 0700 permissions to prevent cross-user access on shared hosts.

```cpp
// Debug mode with 24-hour retention
LLMEngine engine(provider_type, api_key, model, params, 24, true);

// Disable debug files entirely
setenv("LLMENGINE_DISABLE_DEBUG_FILES", "1", 1);
```

## Command Execution Safety

`Utils::execCommand()` uses `posix_spawn()` and does not route through a shell, eliminating shell injection risks:

- No shell involvement: commands are executed via `posix_spawn()` with explicit argv vectors.
- Control characters are rejected: newlines, tabs, carriage returns, and all control characters are blocked.
- Shell metacharacters are rejected: `|`, `&`, `;`, `$`, `` ` ``, `<`, `>`, parentheses, brackets, and wildcards are blocked.
- Whitelist approach: only alphanumeric, single spaces, hyphens, underscores, dots, and slashes are allowed.
- Multiple consecutive spaces are rejected to prevent obfuscation.

Security benefits:
- Bypasses shell interpretation entirely.
- Validation is enforced as defense-in-depth.
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


