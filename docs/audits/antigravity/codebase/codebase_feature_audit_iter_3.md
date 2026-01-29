# Codebase Feature Audit (Iteration 3)

## Existing capabilities that are under-exposed or incomplete

1.  **Streaming Convenience**:
    -   `analyzeStream` requires a manually written callback.
    -   Common patterns (print to stdout, append to string) are boilerplate.
    -   **Missing Feature**: `StreamUtils` with factories for common callbacks.

2.  **Custom Logging Integration**:
    -   `Logger` interface exists, `DefaultLogger` prints to stdio.
    -   Integrating with GUI logs or external systems requires subclassing `Logger` which is slightly verbose for simple cases.
    -   **Missing Feature**: A `CallbackLogger` or `FunctionalLogger` that accepts a lambda `(LogLevel, string)`.

## Summary of Changes Required (Plan for Design)

-   **New Header**: `StreamUtils.hpp` with `toOStream` and `accumulator`.
-   **API Extension**: `CallbackLogger` in `Logger.hpp`.
