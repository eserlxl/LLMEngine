# Codebase Feature Design (Iteration 3)

## Proposed New or Extended APIs

### 1. Stream Utilities
**New Header**: `LLMEngine/StreamUtils.hpp`

```cpp
namespace LLMEngine::StreamUtils {
    /**
     * @brief Creates a callback that writes stream content to an output stream.
     */
    inline StreamCallback toOStream(std::ostream& os);

    /**
     * @brief Creates a callback that accumulates stream content into a string string.
     */
    inline StreamCallback accumulator(std::string& buffer);
}
```

### 2. Functional Logger
**Extension**: `LLMEngine/Logger.hpp`

```cpp
class LLMENGINE_EXPORT CallbackLogger : public Logger {
public:
    using LogCallback = std::function<void(LogLevel, std::string_view)>;
    
    explicit CallbackLogger(LogCallback callback);
    void log(LogLevel level, std::string_view message) override;
};
```

## Backward Compatibility
- Fully compatible (additive).

## Required Tests
1.  **test_stream_utils**: Verify `toOStream` writes to `stringstream` and `accumulator` updates string.
2.  **test_callback_logger**: Verify callback is invoked on log.
