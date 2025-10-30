# FAQ

## Build errors about namespaces
Ensure all references use `::LLMEngineAPI::ProviderType`.

## How do I build and run tests quickly?
Use CMake out-of-source builds and CTest:
```bash
cmake -S . -B build
cmake --build build --config Release -j20
ctest --test-dir build --output-on-failure
```

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

## Where is the configuration loaded from?
See search order in `docs/CONFIGURATION.md` and `config/README.md`.

## How do I generate API documentation?
If your build exposes a docs target:
```bash
cmake -S . -B build
cmake --build build --target docs
```
Open `build/docs/html/index.html` afterwards. See `docs/API_REFERENCE.md`.

## Does LLMEngine support offline mode?
Yes, via `ollama` when running a local Ollama daemon. Online providers require internet access and API keys.

## Logging and troubleshooting
Enable debug mode in the constructor to write artifacts like `api_response.json` and `response_full.txt`. Verify file permissions in your working directory.

## Where is the config file loaded from?
See search order in `docs/CONFIGURATION.md` and `config/README.md`.

