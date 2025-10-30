# Providers

Overview of supported providers and their identifiers.

## Provider Keys

- `qwen` → Qwen (DashScope)
- `openai` → OpenAI
- `anthropic` → Anthropic Claude
- `ollama` → Ollama (local)

## Models (Examples)

- Qwen: `qwen-flash`, `qwen-plus`, `qwen-max`, `qwen2.5-7b-instruct`
- OpenAI: `gpt-3.5-turbo`, `gpt-4o`, `gpt-4.1`
- Anthropic: `claude-3-haiku`, `claude-3-sonnet`, `claude-3-opus`
- Ollama: any locally installed model (e.g., `llama3`, `qwen2.5`)

## Choosing a Provider

Use enum-based or string-based constructors:

```cpp
// Enum
LLMEngine qwen(::LLMEngineAPI::ProviderType::QWEN, apiKey, "qwen-flash");

// String
LLMEngine qwenStr("qwen", apiKey, "qwen-flash");
```

## Defaults From Configuration

When using the string-based constructor without a model, the engine loads `default_model` from `api_config.json`.

```cpp
LLMEngine fromConfig("qwen", apiKey); // uses default_model
```

## Notes

- Online providers require API keys via environment variables.
- Model availability may change; consult provider docs.
- Local Ollama requires the daemon running on `http://localhost:11434` by default.

