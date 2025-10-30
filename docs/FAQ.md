# FAQ

## Build errors about namespaces
Ensure all references use `::LLMEngineAPI::ProviderType`.

## API key not found
Check environment variables are set (e.g., `QWEN_API_KEY`). Avoid hardcoding.

## Rate limit exceeded (429)
- Reduce request frequency
- Implement retries with backoff
- Upgrade your plan

## Timeouts
Increase `timeout_seconds` in `api_config.json` or simplify prompts.

## Thread-safety
Creating separate `LLMEngine` instances per thread is recommended. Avoid sharing mutable state.

## Debug output files not present
Enable debug mode in the constructor and check working directory permissions.

## Local Ollama not responding
Verify the daemon is running at `http://localhost:11434` and model is pulled.

## Which model should I use?
- Fast and cheap: `qwen-flash`
- Balanced: `qwen-plus`
- Most capable: `qwen-max`

## Where is the config file loaded from?
See search order in `docs/CONFIGURATION.md` and `config/README.md`.

