# Providers

Overview of supported providers and their identifiers.

## Feature Matrix

| Provider   | Key       | Type    | Chat/Completions | Streaming | Local Option | Notes |
|------------|-----------|---------|------------------|-----------|--------------|-------|
| Qwen       | `qwen`    | Online  | Yes              | Yes       | No           | Compatible-mode endpoint |
| OpenAI     | `openai`  | Online  | Yes              | Yes       | No           | GPT-3.5/4 family |
| Anthropic  | `anthropic`| Online | Yes              | Yes       | No           | Claude 3 family |
| Ollama     | `ollama`  | Local   | Yes              | Yes       | Yes          | Requires running daemon |
| Gemini     | `gemini`  | Online  | Yes              | No        | No           | Google AI Studio REST |

## Provider Keys

- `qwen` → Qwen (DashScope)
- `openai` → OpenAI
- `anthropic` → Anthropic Claude
- `ollama` → Ollama (local)
- `gemini` → Google Gemini (AI Studio)

## Models (Examples)

- Qwen: `qwen-flash`, `qwen-plus`, `qwen-max`, `qwen2.5-7b-instruct`
- OpenAI: `gpt-3.5-turbo`, `gpt-4o`, `gpt-4.1`
- Anthropic: `claude-3-haiku`, `claude-3-sonnet`, `claude-3-opus`
- Ollama: any locally installed model (e.g., `llama3`, `qwen2.5`)
- Gemini: `gemini-1.5-flash`, `gemini-1.5-pro`

## Internal Mapping

The engine maps provider keys to `::LLMEngineAPI::ProviderType`:

```cpp
// Example mapping usage
LLMEngine q1(::LLMEngineAPI::ProviderType::QWEN, apiKey, "qwen-flash");
LLMEngine q2("qwen", apiKey, "qwen-flash");
```

When using string-based constructors without a model, the `default_model` from `config/api_config.json` is used.

## Choosing a Provider

Enum-based or string-based constructors can be used:

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

See also: `docs/CONFIGURATION.md` for provider defaults and parameters.

