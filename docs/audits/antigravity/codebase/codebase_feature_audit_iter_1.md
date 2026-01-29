# Codebase Feature Audit (Iteration 1)

## 1. Existing Capabilities & Gaps

### LLMEngineBuilder
**Status**: Functional but limited.
**Gaps**:
- Lacks configuration for `max_retries` and `timeout`.
- No typed methods for provider-specific options (e.g., `openai_organization`, `anthropic_version`).
- Does not expose `RequestExecutor` customization (strategy injection) easily without manual dependency injection.

### AnalysisInput
**Status**: Covered basic chat structure.
**Gaps**:
- Missing standard generation parameters (`temperature`, `max_tokens`, `top_p`, `stop_sequences`, `logit_bias`). These are currently only available in `RequestOptions`, forcing a split between "content" and "parameters". They should be available in `AnalysisInput` for self-contained requests.
- `extra_fields` is the only way to pass standard params like `presence_penalty`, which defeats the purpose of strong typing.

### RequestOptions
**Status**: Comprehensive generation options.
**Gaps**:
- `min_p`, `top_k`, `top_logprobs` are present.
- Duplication/ambiguity with `AnalysisInput` needs resolution.

### RequestExecutor
**Status**: `RetryableRequestExecutor` implements retry logic.
**Gaps**:
- `DefaultRequestExecutor` appears to pass options, but `APIClient` support needs verification.
- No `executeBatch` optimization in the executor interface itself (relies on `LLMEngine::analyzeBatch` loop).

### ProviderBootstrap
**Status**: Good environment variable resolution.
**Gaps**:
- Hardcoded defaults for models might need an update or better configuration mechanism.

## 2. Missing Features Implied by Design
- **Typed Provider Configuration**: `GoogleGemini` and `Anthropic` have specific headers/versions that are not easily configurable via the Builder.
- **Cancellation Propagation**: `CancellationToken` exists but needs to be rigorously checked in `APIClient` implementations (cpr/curl callbacks).

## 3. High-Risk Correctness/Safety Issues
- **Parameter Merging**: `ParameterMerger::mergeRequestOptions` needs to strictly define precedence (Input > Options > Defaults).
- **Thread Safety**: `LLMEngine` text specifically mentions it IS thread-safe, but `state_` mutation (like `setLogger`) must be carefully locked.

## 4. API Usability Gaps
- Users must manually create `AnalysisInput` for simple text queries.
- `RequestOptions` is verbose to build.

## 5. Missing Tests
- No specific tests for `CancellationToken` propagation to the network layer.
- `AnalysisInput` validation logic is minimal.

## 6. Immutable/Frozen Areas
- `Constants.hpp`: definitions should remain stable unless new providers are added.
- `APIClient` interface: Stable, but implementations can change.
