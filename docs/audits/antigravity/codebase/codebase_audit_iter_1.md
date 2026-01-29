# Codebase Audit - Iteration 1

## 1. High-risk issues (must fix)
- **Structure Violations**: The `include/` directory contains top-level compatibility headers (`LLMEngine.hpp`, `Logger.hpp`, `RequestLogger.hpp`, `Utils.hpp`, `DebugArtifacts.hpp`) that violate the strict `include/LLMEngine/` namespacing rule.
    - **Resolution**: Remove these headers and update usages.
- **Naming Violations**: Found usage of commented-out `kEventPrefixLen` in `AnthropicClient.cpp` which violates the `camelCase` rule (though it is commented out, it should be removed to be clean).
    - **Resolution**: Remove the commented-out code.

## 2. Medium-risk issues (should fix)
- None identified in this pass.

## 3. Low-risk cleanup
- **CMake Install Rules**: `CMakeLists.txt` installs the compatibility headers. This logic must be removed.

## 4. Missing correctness unit tests
- None identified at this stage (tests seem comprehensive, but needs verification after header removal).

## 5. Tests that verify correctness
- Existing test suite (Unit & Integration tests).

## 6. Areas that should NOT be refactored yet
- `core/LLMEngine.cpp` - Complex logic, avoid unnecessary churn unless breaking.

## 7. Changes since last iteration
- N/A (Iteration 1).
