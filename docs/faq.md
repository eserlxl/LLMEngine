# FAQ

## Build errors about namespaces
Ensure all references use `::LLMEngineAPI::ProviderType`.

## How to build and run tests quickly
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
- Upgrade the API plan

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

## Where the configuration is loaded from
See search order in [docs/configuration.md](configuration.md).

## How to generate API documentation
If the build exposes a docs target:
```bash
cmake -S . -B build
cmake --build build --target docs
```
Open `build/docs/html/index.html` afterwards. See [docs/api_reference.md](api_reference.md) for details.

## Does LLMEngine support offline mode?
Yes, via `ollama` when running a local Ollama daemon. Online providers require internet access and API keys.

## Logging and troubleshooting
Debug mode can be enabled in the constructor to write artifacts like `api_response.json` and `response_full.txt`. File permissions in the working directory should be verified.

## Where the config file is loaded from
See search order in [docs/configuration.md](configuration.md).

## See Also

- [docs/configuration.md](configuration.md) - Configuration details
- [docs/api_reference.md](api_reference.md) - API documentation
- [docs/security.md](security.md) - Security best practices
- [docs/performance.md](performance.md) - Performance tips
- [quickstart.md](../quickstart.md) - Getting started guide
- [README.md](../README.md) - Main documentation

