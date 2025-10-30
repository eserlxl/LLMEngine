# Release Workflow Guide

This guide outlines the automated and manual processes for creating new releases of `next-version`, leveraging semantic versioning and GitHub Actions. It ensures a consistent and transparent release cycle.

## Table of Contents

- [Overview of the Release Process](#overview-of-the-release-process)
- [Quick Start: Creating a Release](#quick-start-creating-a-release)
  - [1. Analyze Changes](#1-analyze-changes)
  - [2. Review Suggested Version Bump](#2-review-suggested-version-bump)
  - [3. Triggering a Release](#3-triggering-a-release)
- [Detailed Release Workflow](#detailed-release-workflow)
  - [Step 1: Prepare for Release](#step-1-prepare-for-release)
  - [Step 2: Analyze Changes for Versioning](#step-2-analyze-changes-for-versioning)
  - [Step 3: Understanding Automatic Release Thresholds](#step-3-understanding-automatic-release-thresholds)
  - [Step 4: Executing the Release](#step-4-executing-the-release)
  - [Step 5: Verifying the Release](#step-5-verifying-the-release)
  - [Step 6: Post-Release Cleanup (Optional)](#step-6-post-release-cleanup-optional)
- [Examples of Release Scenarios](#examples-of-release-scenarios)
- [Troubleshooting Release Issues](#troubleshooting-release-issues)
- [Best Practices for Releases](#best-practices-for-releases)
- [Configuration and Customization](#configuration-and-customization)

## Overview of the Release Process

`next-version` employs a release process primarily driven by [Semantic Versioning (SemVer)](https://semver.org/) and [Conventional Commits](https://www.conventionalcommits.org/). New releases are typically automated via GitHub Actions based on the type of changes merged into the `main` branch. This ensures that version bumps (MAJOR, MINOR, PATCH) accurately reflect the nature of the changes, and release notes are automatically generated.

Key components of the release process include:
-   **`semantic-version-analyzer`**: A utility script (`./bin/semantic-version-analyzer.sh`) that analyzes commit history to suggest the next semantic version bump using the advanced LOC-based delta system.
-   **GitHub Actions Workflow (`version-bump.yml`)**: This workflow automates the version bumping, tag creation, and GitHub Release generation based on detected changes.
-   **Mathematical Version Bumper**: A script (`./bin/mathematical-version-bump.sh`) that handles the actual version increment using the LOC-based delta system.
-   **LOC-Based Delta System**: Advanced versioning that always increases only the patch version with calculated increments based on change magnitude.
-   **Configuration System**: YAML-based configuration (`config/versioning.yml`) for customizing bonus points, multipliers, and thresholds.

[↑ Back to top](#release-workflow-guide)

## Quick Start: Creating a Release

This section provides a condensed guide for quickly creating a release.

### 1. Analyze Changes

Before initiating a release, it's good practice to analyze the changes since the last release to understand the suggested version bump. Use the `semantic-version-analyzer` tool:

```bash
# Analyze changes since the last official release
./bin/semantic-version-analyzer.sh --verbose

# Alternatively, analyze changes since a specific Git tag (e.g., v10.5.10)
./bin/semantic-version-analyzer.sh --since v10.5.10 --verbose

# Get machine-readable JSON output for automation
./bin/semantic-version-analyzer.sh --json

# Restrict analysis to specific paths
./bin/semantic-version-analyzer.sh --only-paths "src/**,include/**" --verbose

# Show only the suggested bump type
./bin/semantic-version-analyzer.sh --suggest-only
```

### 2. Review Suggested Version Bump

The analyzer will output a suggested version bump based on Conventional Commits in the history and the LOC-based delta system:
-   **MAJOR**: Indicates breaking changes (`BREAKING CHANGE` in commit footers) or large-scale changes with high bonus values.
-   **MINOR**: Indicates new features (`feat:` commit type) or medium-scale changes.
-   **PATCH**: Indicates bug fixes (`fix:` commit type) or any other changes that don't qualify for major/minor.
-   **NONE**: No significant changes warranting a version bump.

**Note**: With the LOC-based delta system, all changes result in at least a patch bump, with the actual increment calculated based on the magnitude of changes.

### 3. Triggering a Release

#### Option A: Automatic Release (Recommended for most changes)

For most feature additions, bug fixes, or breaking changes, simply push your finalized changes to the `main` branch. The `version-bump.yml` GitHub Actions workflow is configured to automatically detect significant changes (based on predefined thresholds) and trigger a release.

-   Push your changes to `main`:
    ```bash
    git push origin main
    ```
-   Monitor the GitHub Actions tab to see if the "Auto Version Bump with Semantic Release Notes" workflow is triggered and completes successfully.

**Important**: The workflow automatically ignores changes to certain files to prevent infinite loops:
- `VERSION`
- `doc/VERSIONING.md`
- `doc/TAG_MANAGEMENT.md`
- `doc/RELEASE_WORKFLOW.md`
- `.shellcheckrc`

#### Option B: Manual Release (For specific control or overriding automation)

If you need to manually trigger a release (e.g., for a hotfix, a specific prerelease, or to override the automatic detection):

1.  Navigate to your repository on GitHub.
2.  Go to the **Actions** tab.
3.  Select the workflow named **"Auto Version Bump with Semantic Release Notes"** from the left sidebar.
4.  Click the **"Run workflow"** dropdown button on the right.
5.  Fill in the form:
    -   **Bump type**: Choose `auto` for automatic detection, or explicitly select `major`, `minor`, or `patch`.
    -   **Release notes**: Optionally add custom release notes that will be prepended to the automatically generated notes.
    -   **Prerelease**: Check this box if you are creating a prerelease (e.g., `v10.5.11-beta.1`).
6.  Click **"Run workflow"** to start the release process.

[↑ Back to top](#release-workflow-guide)

## Detailed Release Workflow

This section provides a more in-depth look at each step of the release process.

### Step 1: Prepare for Release

Before initiating any release, ensure your `main` branch is clean, up-to-date, and all intended changes are committed.

```bash
# Ensure your local main branch is synchronized with remote
git checkout main
git pull origin main

# Check for any uncommitted changes
git status

# Verify the current version
cat VERSION
# Expected output: 10.5.12

# Run tests to ensure everything is working
./run_tests.sh
```

### Step 2: Analyze Changes for Versioning

Use the semantic version analyzer to understand the impact of your changes:

```bash
# Basic analysis
./bin/semantic-version-analyzer.sh

# Detailed analysis with file changes
./bin/semantic-version-analyzer.sh --verbose

# Machine-readable output for automation
./bin/semantic-version-analyzer.sh --json | jq '.suggestion'

# Analyze specific time period
./bin/semantic-version-analyzer.sh --since-date 2025-01-01 --verbose

# Show configuration values
./bin/version-config-loader.sh --machine

# Analyze changes since a specific commit
./bin/semantic-version-analyzer.sh --since-commit abc123 --verbose
```

The analyzer will provide:
- Suggested version bump type (major/minor/patch/none)
- LOC-based delta calculations
- Detailed change analysis
- Bonus point calculations
- Rollover warnings if applicable

### Step 3: Understanding Automatic Release Thresholds

The automated release workflow uses intelligent thresholds to ensure every meaningful change results in a version bump:

-   **MAJOR Release**: Triggered by breaking changes, API changes, security issues, or large-scale changes with high bonus values. No size threshold applies as breaking changes are always significant.
-   **MINOR Release**: Triggered by new features, CLI additions, or significant new content (new source files, test files, documentation).
-   **PATCH Release**: Triggered by **any changes** that don't qualify for major or minor bumps. This ensures every change results in at least a patch version increment.
-   **No Release**: Only occurs when there are truly no changes to analyze (e.g., single-commit repositories).

The system uses the LOC-based delta system to calculate the actual version increment, ensuring that even small changes get appropriate version bumps while larger changes get proportionally larger increments.

### Step 4: Executing the Release

#### Automatic Release Process

1. **Push to Main**: Push your changes to the `main` branch
2. **Workflow Trigger**: The `version-bump.yml` workflow automatically triggers
3. **Analysis**: The workflow runs `semantic-version-analyzer` to determine the appropriate version bump
4. **Version Update**: If a bump is warranted, the `mathematical-version-bump.sh` script updates the `VERSION` file
5. **Tag Creation**: A new Git tag is created with the new version
6. **Release Generation**: A GitHub Release is created with automatically compiled release notes
7. **CI/CD Verification**: Comprehensive tests run on the new tag

#### Manual Release Process

1. **Trigger Workflow**: Use the GitHub Actions interface to manually trigger the release workflow
2. **Configure Parameters**: Set the bump type, release notes, and prerelease status
3. **Execute**: The workflow follows the same process as automatic releases
4. **Monitor**: Watch the workflow logs for any issues

### Step 5: Verifying the Release

After a release is created, verify that everything is working correctly:

```bash
# Check the new version
cat VERSION

# Verify the tag was created
git tag --sort=-version:refname | head -5

# Check the release on GitHub
# Visit: https://github.com/eserlxl/next-version/releases

# Test the new version locally
./build.sh
./build/bin/next-version --version

# Run comprehensive tests
./run_tests.sh
```

### Step 6: Post-Release Cleanup (Optional)

For maintenance releases, you may want to clean up old tags:

```bash
# List all tags
./bin/tag-manager.sh list

# Clean up old tags (interactive)
./bin/tag-manager.sh cleanup

# Or keep only the 10 most recent tags
./bin/tag-manager.sh cleanup 10
```

[↑ Back to top](#release-workflow-guide)

## Examples of Release Scenarios

### Scenario 1: Bug Fix Release

**Changes**: Fixed a memory leak in the log processing module (50 LOC changed)

**Analysis**:
```bash
./bin/semantic-version-analyzer.sh --verbose
# Output: SUGGESTION=patch
# LOC: 50, Base PATCH: 1, Final PATCH: 1
```

**Result**: 10.5.12 → 10.5.13 (patch bump)

### Scenario 2: Feature Addition

**Changes**: Added new `--depth` option for controlling recursion depth (200 LOC changed, new CLI option)

**Analysis**:
```bash
./bin/semantic-version-analyzer.sh --verbose
# Output: SUGGESTION=minor
# LOC: 200, Base MINOR: 5, Bonus: CLI changes (+2), Final MINOR: 7
```

**Result**: 10.5.12 → 10.5.19 (patch bump with minor-level delta)

### Scenario 3: Breaking Change

**Changes**: Removed deprecated `--old-format` option (100 LOC changed, breaking CLI change)

**Analysis**:
```bash
./bin/semantic-version-analyzer.sh --verbose
# Output: SUGGESTION=major
# LOC: 100, Base MAJOR: 10, Bonus: Breaking CLI (+2), Final MAJOR: 12
```

**Result**: 10.5.12 → 10.5.24 (patch bump with major-level delta)

### Scenario 4: Security Fix

**Changes**: Fixed buffer overflow vulnerability (150 LOC changed, security keywords detected)

**Analysis**:
```bash
./bin/semantic-version-analyzer.sh --verbose
# Output: SUGGESTION=patch
# LOC: 150, Base PATCH: 1, Bonus: Security keywords (3×+2), Final PATCH: 7
```

**Result**: 10.5.12 → 10.5.19 (patch bump)

### Scenario 5: Large Refactoring

**Changes**: Major refactoring of the core processing engine (2000 LOC changed)

**Analysis**:
```bash
./bin/semantic-version-analyzer.sh --verbose
# Output: SUGGESTION=major
# LOC: 2000, Base MAJOR: 30, Final MAJOR: 30
```

**Result**: 10.5.12 → 10.5.42 (patch bump with major-level delta)

### Scenario 6: Performance Improvement

**Changes**: Optimized log parsing algorithm with 25% performance improvement (300 LOC changed)

**Analysis**:
```bash
./bin/semantic-version-analyzer.sh --verbose
# Output: SUGGESTION=minor
# LOC: 300, Base MINOR: 5, Bonus: Performance 20-50% (+2), Final MINOR: 7
```

**Result**: 10.5.12 → 10.5.19 (patch bump with minor-level delta)

[↑ Back to top](#release-workflow-guide)

## Troubleshooting Release Issues

### Common Issues and Solutions

#### Issue: Release Not Triggered
**Symptoms**: Changes pushed to main but no release workflow runs
**Solutions**:
- Check if changes meet the automatic release thresholds
- Verify commit messages follow Conventional Commits format
- Review GitHub Actions workflow configuration
- Check workflow logs for errors
- Ensure changes are not to ignored files (VERSION, doc files, .shellcheckrc)

#### Issue: Incorrect Version Bump
**Symptoms**: Version bump doesn't match expected change type
**Solutions**:
- Run `./bin/semantic-version-analyzer.sh --verbose` to understand the analysis
- Check LOC calculations and bonus point assignments
- Review the LOC-based delta system configuration
- Consider manual release with specific bump type
- Verify the configuration in `config/versioning.yml`

#### Issue: Tag Conflicts
**Symptoms**: Workflow fails due to existing tag
**Solutions**:
- Check for existing tags with the same version
- Use `./bin/tag-manager.sh list` to see all tags
- Clean up conflicting tags if necessary
- Ensure no manual tags conflict with automated releases

#### Issue: Release Notes Issues
**Symptoms**: Generated release notes are incomplete or incorrect
**Solutions**:
- Verify commit messages are properly formatted
- Check for conventional commit types (feat:, fix:, BREAKING CHANGE:)
- Review the release notes generation logic
- Consider adding custom release notes

#### Issue: Workflow Concurrency Conflicts
**Symptoms**: Multiple version bump workflows running simultaneously
**Solutions**:
- The workflow includes concurrency guards to prevent conflicts
- Check if another workflow is already running
- Wait for the current workflow to complete
- Review workflow logs for concurrency issues

#### Issue: Mathematical Version Bump Script Not Found
**Symptoms**: Workflow fails with "mathematical-version-bump.sh not found"
**Solutions**:
- Ensure the script exists at `./bin/mathematical-version-bump.sh`
- Check script permissions (should be executable)
- Verify the script is committed to the repository
- Check for any path issues in the workflow

### Debugging Commands

```bash
# Check current version and recent tags
cat VERSION
git tag --sort=-version:refname | head -5

# Analyze changes in detail
./bin/semantic-version-analyzer.sh --verbose --json | jq '.'

# Check workflow configuration
cat .github/workflows/version-bump.yml

# Validate configuration
./bin/version-config-loader.sh --machine

# Test rollover logic
./test-workflows/core-tests/test_rollover_logic.sh

# Run comprehensive versioning tests
./test-workflows/test_semantic_version_analyzer_comprehensive.sh

# Check for ignored files in workflow
git log --oneline --since="1 day ago" --name-only

# Test mathematical version bump locally
./bin/mathematical-version-bump.sh --help

# Check script permissions
ls -la ./bin/semantic-version-analyzer.sh
ls -la ./bin/mathematical-version-bump.sh
```

[↑ Back to top](#release-workflow-guide)

## Best Practices for Releases

### Commit Message Guidelines

Follow [Conventional Commits](https://www.conventionalcommits.org/) for consistent version detection:

```bash
# Feature addition
feat: add new --depth option for recursion control

# Bug fix
fix: resolve memory leak in log processing

# Breaking change
feat!: remove deprecated --old-format option

BREAKING CHANGE: The --old-format option has been removed.
Use --new-format instead.

# Documentation update
docs: update installation instructions

# Performance improvement
perf: optimize log parsing algorithm

# Security fix
fix: patch buffer overflow vulnerability

# Refactoring
refactor: restructure log processing pipeline
```

### Release Planning

1. **Feature Freeze**: Stop adding new features before a major release
2. **Testing**: Ensure comprehensive testing before release
3. **Documentation**: Update documentation to reflect changes
4. **Release Notes**: Review automatically generated release notes
5. **Prereleases**: Use prereleases for major changes (e.g., `v11.0.0-beta.1`)

### Version Management

1. **Avoid Manual Version Changes**: Let the automated system handle version bumps
2. **Monitor LOC-Based Deltas**: Understand how the delta system affects version increments
3. **Use Configuration**: Leverage the YAML configuration system for consistent behavior
4. **Regular Cleanup**: Periodically clean up old tags to maintain repository health
5. **Understand Rollover Logic**: Be aware of the rollover system for large changes

### Quality Assurance

1. **Pre-Release Testing**: Run the full test suite before release
2. **Integration Testing**: Test the release in a staging environment
3. **Documentation Review**: Ensure documentation is up-to-date
4. **Release Verification**: Verify the release works as expected
5. **Performance Testing**: Ensure performance improvements are measurable

### Communication

1. **Release Announcements**: Use GitHub Releases for announcements
2. **Breaking Changes**: Clearly communicate breaking changes
3. **Migration Guides**: Provide migration guides for major releases
4. **Support**: Be prepared to support users during the release
5. **Changelog**: Review and enhance automatically generated release notes

[↑ Back to top](#release-workflow-guide)

## Configuration and Customization

### Versioning Configuration

The versioning system can be customized through `config/versioning.yml`:

```yaml
# Base deltas for different change types
base_deltas:
  patch: "1"
  minor: "5"
  major: "10"

# LOC cap and rollover configuration
limits:
  loc_cap: 10000
  rollover: 100

# Bonus system for different change types
bonuses:
  security_stability:
    security_vuln: 5
    cve: 2
    memory_safety: 4
  features:
    new_cli_command: 2
    new_config_option: 1
  # ... more categories
```

### Customizing Bonus Points

To adjust the versioning behavior for your project:

1. **Edit Configuration**: Modify `config/versioning.yml`
2. **Test Changes**: Use `./bin/version-config-loader.sh --machine` to verify
3. **Run Tests**: Execute `./test-workflows/test_semantic_version_analyzer_comprehensive.sh`
4. **Commit Changes**: Include configuration updates in your release

### Workflow Customization

The GitHub Actions workflow can be customized by editing `.github/workflows/version-bump.yml`:

- **Trigger Conditions**: Modify the `on.push.paths-ignore` section
- **Environment Variables**: Add custom environment variables
- **Additional Steps**: Include custom verification or notification steps
- **Permissions**: Adjust repository permissions as needed

### Testing Configuration Changes

Before applying configuration changes to production:

```bash
# Test with current configuration
./bin/semantic-version-analyzer.sh --verbose

# Test with modified configuration
./test-workflows/core-tests/test_version_logic.sh

# Run comprehensive tests
./test-workflows/run_workflow_tests.sh

# Test mathematical version bump
./bin/mathematical-version-bump.sh --help
```

[↑ Back to top](#release-workflow-guide)