# Change Notes

This file stores rationale and design decisions for code generation tasks. Update this file when making significant changes to document the reasoning behind design choices.

## Template

Use this template when documenting changes:

```markdown
## Date: YYYY-MM-DD

### Task: [Brief description]

**Why these choices:**

1. **Decision 1**: Explanation of why this approach was chosen
2. **Decision 2**: Explanation of alternative approaches considered
3. **Decision 3**: Any trade-offs made

**Alternatives considered:**

- Alternative A: Why it was rejected
- Alternative B: Why it was rejected

**References:**

- See `.cursor/rules/XX-*.md` for relevant guidelines
- External references if applicable
```

## Example

```markdown
## Date: 2025-01-15

### Task: Add retry logic to API client

**Why these choices:**

1. **Exponential backoff**: Prevents overwhelming server with retries during outages. Provides progressive delays that give the server time to recover.
2. **Configurable max retries**: Allows customization per use case. Some operations may need more retries than others.
3. **std::expected for errors**: Type-safe error handling using C++23 feature. Makes error paths explicit and testable.

**Alternatives considered:**

- Fixed retry count: Less flexible, doesn't adapt to different scenarios
- Linear backoff: Less efficient for network issues, exponential is standard practice
- Exceptions for errors: Violates error handling policy (see `20-error-handling.md`)

**References:**

- See `20-error-handling.md` for error handling patterns
- See `30-performance.md` for performance considerations
- See `10-cpp-style.md` for C++ style guidelines
```

## Notes

- Update this file for significant changes or when design decisions need documentation
- Keep entries concise but informative
- Reference relevant rule files when applicable
- Document trade-offs and alternatives considered

