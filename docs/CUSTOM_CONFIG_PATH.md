# Custom Config File Path

This document explains how LLMEngine consumers can set custom configuration file paths programmatically.

## Overview

LLMEngine provides multiple ways for consumers to specify the location of the configuration file (`api_config.json`). This flexibility allows applications to:
- Use configuration files in custom locations
- Support multiple configuration files for different environments
- Integrate with application-specific configuration management systems

## Config File Search Order

The library searches for configuration files in the following order:

1. **Explicit path** passed to `loadConfig(path)` in application code
2. **Default path** set via `setDefaultConfigPath()` (defaults to `config/api_config.json`)
3. **Installation path**: `/usr/share/llmEngine/config/api_config.json` (after installation)

## Methods to Set Config File Path

### Method 1: Set Default Config Path (Recommended)

Use `APIConfigManager::setDefaultConfigPath()` to change the default path that will be used when `loadConfig()` is called without arguments.

```cpp
#include "LLMEngine/APIClient.hpp"

using namespace LLMEngineAPI;

// Get the singleton config manager instance
auto& config_mgr = APIConfigManager::getInstance();

// Set a custom default path
config_mgr.setDefaultConfigPath("/custom/path/api_config.json");

// Query the current default path
std::string path = config_mgr.getDefaultConfigPath();  
// Returns "/custom/path/api_config.json"

// Load using the default path
config_mgr.loadConfig();  // Uses "/custom/path/api_config.json"
```

**When to use:** This method is ideal when an application wants to set a single default configuration path that will be used throughout the application lifecycle.

### Method 2: Explicit Path in loadConfig()

Pass an explicit path directly when loading the configuration. This overrides any default path that may have been set.

```cpp
#include "LLMEngine/APIClient.hpp"

using namespace LLMEngineAPI;

auto& config_mgr = APIConfigManager::getInstance();

// Load from a specific path (overrides default)
bool success = config_mgr.loadConfig("/another/path/config.json");

if (!success) {
    // Handle configuration load failure
    std::cerr << "Failed to load configuration from /another/path/config.json\n";
}
```

**When to use:** This method is useful when loading different configuration files for different scenarios (e.g., development vs. production, different tenants, or A/B testing).

### Method 3: Dependency Injection via Constructor

Pass a custom `IConfigManager` implementation to the `LLMEngine` constructor. This allows complete control over configuration management, including custom implementations for testing or advanced use cases.

```cpp
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/APIClient.hpp"

using namespace LLMEngine;
using namespace LLMEngineAPI;

// Option A: Use the singleton with custom path
auto& config_mgr = APIConfigManager::getInstance();
config_mgr.setDefaultConfigPath("/custom/path/api_config.json");
config_mgr.loadConfig();

// Wrap singleton in shared_ptr (with no-op deleter since it's a singleton)
auto config_manager = std::shared_ptr<IConfigManager>(
    &config_mgr, 
    [](IConfigManager*) {}  // No-op deleter
);

// Pass to LLMEngine constructor
LLMEngine::LLMEngine engine(
    "qwen",           // provider_name
    "",               // api_key (optional)
    "",               // model (optional, uses config default)
    {},               // model_params
    24,               // log_retention_hours
    false,            // debug
    config_manager    // custom config manager
);
```

**When to use:** This method is useful for:
- Dependency injection in testing scenarios
- Using custom `IConfigManager` implementations
- Isolating configuration management per `LLMEngine` instance

## Complete Example

Here's a complete example demonstrating all three methods:

```cpp
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/APIClient.hpp"
#include <iostream>

using namespace LLMEngine;
using namespace LLMEngineAPI;

int main() {
    try {
        // Method 1: Set default path
        auto& config_mgr = APIConfigManager::getInstance();
        config_mgr.setDefaultConfigPath("/etc/myapp/api_config.json");
        
        // Load using default path
        if (!config_mgr.loadConfig()) {
            std::cerr << "Failed to load default config\n";
            return 1;
        }
        
        // Method 2: Load from explicit path (overrides default)
        if (!config_mgr.loadConfig("/tmp/test_config.json")) {
            std::cerr << "Failed to load test config\n";
            // Fall back to default
            config_mgr.loadConfig();
        }
        
        // Method 3: Use with LLMEngine constructor
        LLMEngine::LLMEngine engine(
            "qwen",
            "",  // API key from environment variable
            "",  // Model from config
            {},
            24,
            false,
            std::shared_ptr<IConfigManager>(&config_mgr, [](IConfigManager*) {})
        );
        
        // Use the engine
        AnalysisResult result = engine.analyze(
            "Hello, world!",
            nlohmann::json::object(),
            "greeting"
        );
        
        std::cout << result.content << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

## Thread Safety

All `APIConfigManager` methods are **thread-safe** and can be called concurrently from multiple threads. The implementation uses read-write locks to ensure safe concurrent access.

```cpp
// Safe to call from multiple threads
std::thread t1([&]() {
    auto& mgr = APIConfigManager::getInstance();
    mgr.setDefaultConfigPath("/path1/config.json");
    mgr.loadConfig();
});

std::thread t2([&]() {
    auto& mgr = APIConfigManager::getInstance();
    mgr.setDefaultConfigPath("/path2/config.json");
    mgr.loadConfig();
});
```

## API Reference

### APIConfigManager Methods

#### `setDefaultConfigPath(std::string_view config_path)`

Sets the default configuration file path that will be used when `loadConfig()` is called without arguments.

- **Parameters:**
  - `config_path`: The path to use as the default configuration file
- **Thread Safety:** Thread-safe (uses exclusive lock)
- **Default:** `"config/api_config.json"`

#### `getDefaultConfigPath() const`

Returns the current default configuration file path.

- **Returns:** `std::string` - The current default config path
- **Thread Safety:** Thread-safe (uses shared lock)

#### `loadConfig(std::string_view config_path = "")`

Loads configuration from the specified path or from the default path if no path is provided.

- **Parameters:**
  - `config_path`: Optional explicit path. If empty, uses the default path set via `setDefaultConfigPath()`
- **Returns:** `bool` - `true` if configuration loaded successfully, `false` otherwise
- **Thread Safety:** Thread-safe (uses exclusive lock for writing, shared lock for reading default path)

## Best Practices

1. **Set the default path early:** Configure the default path at application startup before creating `LLMEngine` instances.

2. **Handle load failures:** Always check the return value of `loadConfig()` and handle failures appropriately.

3. **Use environment-specific paths:** Consider using different configuration paths for development, staging, and production environments.

4. **Prefer explicit paths for testing:** In test code, use explicit paths to avoid dependencies on default configuration locations.

5. **Thread safety:** While the API is thread-safe, be aware that changing the default path affects all subsequent `loadConfig()` calls without arguments.

## Related Documentation

- [CONFIGURATION.md](CONFIGURATION.md) - Complete configuration guide
- [API_REFERENCE.md](API_REFERENCE.md) - Full API documentation
- [ARCHITECTURE.md](ARCHITECTURE.md) - Architecture and design details

