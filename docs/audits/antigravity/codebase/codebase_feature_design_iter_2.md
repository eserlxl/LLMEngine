# Codebase Feature Design (Iteration 2)

## Proposed New or Extended APIs

### 1. Tool Choice Helpers
**New Struct**: `ToolChoice` (Header-only in `AnalysisInput.hpp` or new `ToolChoice.hpp`)

```cpp
struct ToolChoice {
    static nlohmann::json none();
    static nlohmann::json autoChoice(); // 'auto' is a keyword
    static nlohmann::json required();   // 'any' or 'required' depending on provider, usually 'required'
    static nlohmann::json function(std::string_view name);
};
```
*Note: We return `nlohmann::json` directly to be compatible with `AnalysisInput::tool_choice`. wrapper class isn't strictly necessary if helpers are static.*

### 2. Tool Call Argument Parsing
**Extension**: `AnalysisResult::ToolCall`

```cpp
struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;

    // [NEW]
    [[nodiscard]] std::optional<nlohmann::json> getArguments() const {
        try {
            return nlohmann::json::parse(arguments);
        } catch (...) {
            return std::nullopt;
        }
    }
};
```

### 3. CancellationToken Factory
**Extension**: `LLMEngine` class

```cpp
class LLMEngine {
public:
    // ...
    [[nodiscard]] static std::shared_ptr<CancellationToken> createCancellationToken();
};
```

## Error Handling
- `getArguments()` returns `std::nullopt` on parse failure, avoiding exceptions in accessors.

## Backward Compatibility
- Fully compatible. New methods are additive.

## Required Tests
1.  **test_tool_choice**: Verify JSON output of `ToolChoice` helpers matches expected API format.
2.  **test_tool_call_parsing**: Verify `getArguments()` parses valid JSON and handles invalid JSON gracefully.
