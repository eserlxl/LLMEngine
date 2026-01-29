# Codebase Feature Design (Iteration 1)

## 1. Objective
Address the functional gaps identified in the audit:
1.  Incomplete `AnalysisInput` for standard parameters.
2.  Missing configuration in `LLMEngineBuilder`.
3.  Inconsistent `CancellationToken` support in implementation.

## 2. API Changes

### 2.1 AnalysisInput Extensions
Add standard LLM parameters directly to `AnalysisInput` to allow self-contained requests without relying on side-channel `RequestOptions`.

```cpp
struct AnalysisInput {
    // ... existing fields ...
    
    // New Standard Generation Parameters
    std::optional<double> temperature;
    std::optional<int> max_tokens;
    std::optional<double> top_p;
    std::vector<std::string> stop_sequences;
    std::optional<nlohmann::json> logit_bias;
    std::optional<double> frequency_penalty;
    std::optional<double> presence_penalty;

    // Builder methods for new fields
    AnalysisInput& setTemperature(double t);
    AnalysisInput& setMaxTokens(int t);
    // ... etc
};
```

### 2.2 LLMEngineBuilder Extensions
Add missing configuration methods.

```cpp
class LLMEngineBuilder {
public:
    // ... existing ...
    
    LLMEngineBuilder& withTimeout(std::chrono::milliseconds timeout);
    LLMEngineBuilder& withMaxRetries(int retries);
    
    // Generic provider option storage
    template <typename T>
    LLMEngineBuilder& withProviderOption(std::string_view key, const T& value);
};
```

### 2.3 Parameter Merging Strategy
Define explicit precedence in `ParameterMerger`:
1.  **Explicit RequestOptions** (Highest priority)
2.  **AnalysisInput** fields
3.  **LLMEngine/Provider Defaults** (Lowest priority)

### 2.4 Cancellation Design
Formalize cancellation support in `APIClient` implementations.
-   **OpenAI/Anthropic/Gemini/Qwen**: Use `cpr::SetProgressCallback` to check `token->isCancelled()` during transfer.
-   **Ollama**: Same approach.

## 3. Implementation Plan
1.  **Modify `AnalysisInput` header**: Add fields and builder methods.
2.  **Modify `AnalysisInput.cpp`**: Implement builder methods and JSON serialization.
3.  **Modify `LLMEngineBuilder`**: Add new fields to builder state and pass to `LLMEngine`.
4.  **Update `APIClient` implementations**: Ensure they respect `RequestOptions` for timeout and cancellation.
5.  **Refactor `ParameterMerger`**: Implement the strict precedence logic.

## 4. Verification
-   **Compilation Check**: Ensure no ABI breakage for existing symbols (though API expansion is allowed).
-   **Unit Tests**:
    -   Test `AnalysisInput` builder.
    -   Test `ParameterMerger` precedence.
    -   Mock `APIClient` to verify timeout propagation.
