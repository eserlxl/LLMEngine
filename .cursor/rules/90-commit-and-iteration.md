# Commit and Iteration Rules

## Overview

This document defines commit and iteration guidelines for LLMEngine. These rules ensure clean git history, atomic changes, and effective collaboration.

## Commit Message Format

### Conventional Commits

Use [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Commit Types

- **feat**: New feature
- **fix**: Bug fix
- **docs**: Documentation changes
- **style**: Code style changes (formatting, no logic change)
- **refactor**: Code refactoring (no feature change, no bug fix)
- **test**: Adding or updating tests
- **chore**: Maintenance tasks (build, config, dependencies)
- **perf**: Performance improvements
- **ci**: CI/CD changes

### Examples

```
feat(client): add retry logic for failed requests

Implements exponential backoff with configurable max retries.
Adds RequestRetry class with customizable retry policies.

Closes #123
```

```
fix(parser): handle empty JSON objects correctly

Previously, empty JSON objects were incorrectly parsed as errors.
Now correctly handle {} as valid empty object.

Fixes #456
```

```
chore(deps): update nlohmann_json to v3.11.3

Updates dependency to latest stable version with bug fixes.
No API changes required.

Refs #789
```

## Atomic Commits

### One Logical Change Per Commit

- **Single purpose**: Each commit should have one clear purpose
- **Complete change**: Commit should be complete and functional
- **Reversible**: Commits should be easy to revert

**Good**: Separate commits for feature and tests
```
feat(client): add retry logic
test(client): add tests for retry logic
```

**Bad**: Mixed changes in one commit
```
feat(client): add retry and fix parser bug
```

### Build and Test Before Commit

- **Compiles**: Code must compile without warnings (with `-Werror`)
- **Tests pass**: All tests must pass
- **Formatting**: Code must be formatted (see `80-formatting.md`)

See `cxx-strict-build-before-commit.yaml` for build requirements.

## Task Progression

### After Each Completed Task

1. **Show diff summary**: Brief summary of changes
2. **Checklist**: Verify all requirements met
3. **Commit**: Create atomic commit with proper message
4. **Ask to proceed**: Ask if ready to proceed to next task

### Diff Summary Format

```
## Summary

- Added: Feature X with Y and Z
- Modified: Updated component A
- Removed: Deprecated function B

## Checklist

- [x] Code compiles with -Werror
- [x] Tests pass
- [x] Code formatted
- [x] Documentation updated
- [ ] Ready to commit

Proceed to next task?
```

### Commit Checklist

Before committing, verify:

- [ ] Code compiles (Debug and Release)
- [ ] All tests pass
- [ ] Code formatted (clang-format)
- [ ] No clang-tidy warnings (if enabled)
- [ ] Documentation updated (if API changed)
- [ ] Commit message follows Conventional Commits
- [ ] Changes are atomic and reversible

## Never Batch Unrelated Changes

### Separate Concerns

- **Feature changes**: Separate from bug fixes
- **Refactoring**: Separate from feature additions
- **Tests**: Can be in same commit as feature, or separate
- **Documentation**: Separate if extensive, or include with feature

**Good**: Multiple focused commits
```
feat(api): add new endpoint
fix(parser): handle edge case
docs(api): update API documentation
```

**Bad**: One large commit with everything
```
feat: add endpoint, fix parser, update docs
```

## Auto-Commit Rules

### When Auto-Commit is Active

If `auto-commit-after-task.yaml` is active:

- **Automatic commit**: Commit after each task completion
- **No confirmation**: Proceed without asking
- **Format**: `chore(plan): Task: <title> completed`

### When Auto-Commit is Inactive

- **Manual commit**: Ask before committing
- **Show diff**: Display changes before committing
- **Confirm**: Wait for user confirmation

## Change Notes

### Rationale Documentation

Store rationale for code generation decisions in `.cursor/last-change-notes.md`:

```markdown
# Change Notes

## Date: 2026-01-XX

### Feature: Retry Logic

**Why these choices:**

1. **Exponential backoff**: Prevents overwhelming server with retries
2. **Configurable max retries**: Allows customization per use case
3. **std::expected for errors**: Type-safe error handling (C++23)

**Alternatives considered:**

- Fixed retry count: Less flexible
- Linear backoff: Less efficient for network issues

**References:**

- See `20-error-handling.md` for error handling patterns
- See `30-performance.md` for performance considerations
```

## Commit Workflow

### Standard Workflow

1. **Make changes**: Implement feature/fix
2. **Build**: Ensure code compiles
3. **Test**: Run tests, verify they pass
4. **Format**: Apply clang-format
5. **Review**: Check diff, verify checklist
6. **Commit**: Create commit with proper message
7. **Proceed**: Ask to proceed to next task

### Error Handling

If build or tests fail:

1. **Fix immediately**: Don't commit broken code
2. **Rebuild**: Verify fixes work
3. **Re-test**: Ensure tests pass
4. **Then commit**: Only commit working code

## Commit Message Guidelines

### Subject Line

- **Length**: 50 characters or less (recommended)
- **Capitalization**: First word capitalized
- **No period**: No trailing period
- **Imperative mood**: "Add feature" not "Added feature"

### Body

- **Length**: Wrap at 72 characters
- **Content**: Explain what and why, not how
- **Blank line**: Separate subject from body
- **Bullet points**: Use for multiple changes

### Footer

- **Issue references**: `Closes #123`, `Fixes #456`
- **Breaking changes**: `BREAKING CHANGE: description`
- **Co-authors**: `Co-authored-by: Name <email>`

## Summary

1. **Conventional Commits**: Use standard format
2. **Atomic commits**: One logical change per commit
3. **Build and test**: Always verify before commit
4. **Diff summary**: Show changes before commit
5. **Never batch**: Separate unrelated changes
6. **Change notes**: Document rationale for decisions
7. **Checklist**: Verify all requirements before commit

## References

- See `cxx-strict-build-before-commit.yaml` for build requirements
- See `auto-commit-after-task.yaml` for auto-commit behavior
- See `99-review-checklist.md` for pre-commit checklist
- [Conventional Commits](https://www.conventionalcommits.org/)

