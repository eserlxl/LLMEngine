# Performance

Guidelines to improve latency, determinism, and cost when using LLMEngine.

## Quick Tips

- Use `qwen-flash` for fastest responses (when available for your use case).
- Reduce `max_tokens` to decrease latency and cost.
- Lower `temperature` for more deterministic output.

## Example

```cpp
nlohmann::json input = {{"max_tokens", 500}};
AnalysisResult result = engine.analyze(prompt, input, "test");
if (!result.success) {
    std::cerr << "Error: " << result.errorMessage << " (status " << result.statusCode << ")\n";
}
```

## Additional Notes

- Network latency and provider load can dominate total time; choose the nearest region where supported.
- Prefer streaming (if/when supported in your integration) to start processing partial results earlier.
- Use sensible defaults from `config/api_config.json`, overriding only when necessary for your workload.

## See Also

- [docs/CONFIGURATION.md](CONFIGURATION.md) - Configuration and parameter tuning
- [docs/PROVIDERS.md](PROVIDERS.md) - Provider selection and model recommendations
- [docs/FAQ.md](FAQ.md) - Performance-related questions
- [QUICKSTART.md](../QUICKSTART.md) - Quick start with performance tips
- [README.md](../README.md) - Main documentation

