# Versioning Strategy

This document details the versioning strategy employed by `next-version`, which adheres to [Semantic Versioning (SemVer)](https://semver.org/) principles with an advanced LOC-based delta system. It covers how versions are structured, stored, and how changes are automatically and manually managed to ensure a clear and consistent release history.

## Table of Contents

- [Semantic Versioning Overview](#semantic-versioning-overview)
- [New Versioning System](#new-versioning-system)
- [Version Storage and Display](#version-storage-and-display)
- [Automated Semantic Version Bumping](#automated-semantic-version-bumping)
  - [Automatic Release Detection Thresholds](#automatic-release-detection-thresholds)
  - [Semantic Version Analyzer Tool](#semantic-version-analyzer-tool)
- [Manual Version Management](#manual-version-management)
- [Version Management Tools](#version-management-tools)
- [Release Process Integration](#release-process-integration)
- [Version History](#version-history)
- [Best Practices for Versioning](#best-practices-for-versioning)
- [Troubleshooting Versioning Issues](#troubleshooting-versioning-issues)

## Semantic Versioning Overview

`next-version` strictly follows the [Semantic Versioning 2.0.0](https://semver.org/) specification, using the `MAJOR.MINOR.PATCH` format:

-   **MAJOR Version (X.0.0)**: Incremented for incompatible API changes. This signifies that users might need to adapt their code or usage patterns when upgrading.
-   **MINOR Version (0.Y.0)**: Incremented for adding new functionality in a backward-compatible manner. Existing usage should continue to work without modification.
-   **PATCH Version (0.0.Z)**: Incremented for backward-compatible bug fixes and minor internal improvements. These changes should not affect existing functionality.

### Version Format Examples

-   `10.5.12` → `10.5.13`: A bug fix or minor internal improvement.
-   `10.5.12` → `10.6.0`: A new feature was added, but existing functionality remains compatible.
-   `10.5.12` → `11.0.0`: A breaking change was introduced, requiring users to update their integration.

**Note**: The current versioning system uses an advanced LOC-based delta system that always increases only the last identifier (patch) with calculated increments based on change magnitude.

[↑ Back to top](#versioning-strategy)

## New Versioning System

`next-version` implements an advanced versioning system that **always increases only the last identifier (patch)** with a delta calculated based on the magnitude of changes. This system prevents version number inflation while maintaining semantic meaning.

### Core Principles

1. **Always Increase Only the Last Identifier**: All version changes increment only the patch version (the last number)
2. **LOC-Based Delta Calculation**: The increment amount is calculated based on Lines of Code (LOC) changed plus bonus additions
3. **Rollover Logic**: Uses mod 1000 for patch and minor version limits with automatic rollover
4. **Enhanced Reason Format**: Includes LOC value and version type in analysis output
5. **Universal Patch Detection**: Every change results in at least a patch bump
6. **Pure Mathematical Logic**: No arbitrary rules - all calculations follow mathematical formulas

### Delta Formulas

The system uses the following formulas to calculate version increments:

```bash
# Base delta from LOC
PATCH: 1 * (1 + LOC/250)  # Small changes get small increments
MINOR: 5 * (1 + LOC/500)  # Medium changes get medium increments  
MAJOR: 10 * (1 + LOC/1000) # Large changes get large increments

# Bonus multiplication with LOC gain
Bonus Multiplier: (1 + LOC/L) where L depends on version type
Total Delta: base_delta + (bonus * bonus_multiplier)
```

### Rollover System

The new system implements intelligent rollover logic with `MAIN_VERSION_MOD = 1000`:

- **Patch rollover**: When patch + delta >= 1000, apply mod 1000 and increment minor
- **Minor rollover**: When minor + 1 >= 1000, apply mod 1000 and increment major
- **Example**: 10.5.995 + 6 = 10.6.1 (patch rollover)
- **Example**: 10.999.995 + 6 = 11.0.1 (minor rollover)

### Examples

#### Small Change (100 LOC) - No Bonuses
```bash
Base PATCH: 1 * (1 + 100/250) = 1.4 → 1
Bonus Multiplier: 1 + 100/250 = 1.4
Total Delta: 1 + (0 * 1.4) = 1

Result: 10.5.12 → 10.5.13 (patch)
```

#### Medium Change (500 LOC) with CLI Additions
```bash
Base MINOR: 5 * (1 + 500/500) = 10
Bonus: CLI changes (+2) + Added options (+1) = +3
Bonus Multiplier: 1 + 500/500 = 2.0
Total Bonus: 3 * 2.0 = 6
Total Delta: 10 + 6 = 16

Result: 10.5.12 → 10.5.28 (patch with minor-level delta)
```

#### Large Change (2000 LOC) with Breaking Changes
```bash
Base MAJOR: 10 * (1 + 2000/1000) = 30
Bonus: Breaking CLI (+4) + API breaking (+5) + Removed features (+3) = +12
Bonus Multiplier: 1 + 2000/1000 = 3.0
Total Bonus: 12 * 3.0 = 36
Total Delta: 30 + 36 = 66

Result: 10.5.12 → 10.5.78 (patch with major-level delta)
```

#### Security Fix (100 LOC) with Security Keywords
```bash
Base PATCH: 1 * (1 + 100/250) = 1.4 → 1
Bonus: Security vulnerability (+5) + CVE (+2) = +7
Bonus Multiplier: 1 + 100/250 = 1.4
Total Bonus: 7 * 1.4 = 9.8 → 9
Total Delta: 1 + 9 = 10

Result: 10.5.12 → 10.5.22 (patch)
```

#### Rollover Examples
```bash
# Patch rollover
10.5.995 + 6 = 10.6.1

# Minor rollover  
10.999.995 + 6 = 11.0.1

# Double rollover
10.999.999 + 1 = 11.0.0
```

### Enhanced Reason Format

The system now provides enhanced analysis output that includes:
- **LOC value**: The actual lines of code changed
- **Version type**: MAJOR, MINOR, or PATCH
- **Base delta calculation**: The calculated base increment
- **Bonus multiplication**: How bonuses are multiplied by LOC gain
- **Total delta**: The final increment amount
- **Example**: "cli_added (LOC: 200, MINOR, base_delta=5, bonus=3*1.4=4, total_delta=9)"

For more details on the LOC-based delta system, see [LOC Delta System Documentation](LOC_DELTA_SYSTEM.md).

[↑ Back to top](#versioning-strategy)

## Version Storage and Display

The current official version of `next-version` is stored in a plain text file named `VERSION` located at the project root. This file is the single source of truth for the project's version.

### Displaying the Current Version

Users can retrieve the current version of the `next-version` executable at runtime using the `--version` flag:

```bash
next-version --version
# Expected Output: next-version 10.5.12
```

### Version Resolution Order

The `next-version` tool reads the version from the project root `VERSION` file (current working directory) when available.

If the `VERSION` file is not found or accessible in any of these locations, the version will be displayed as "unknown".

[↑ Back to top](#versioning-strategy)

## Automated Semantic Version Bumping

`next-version` employs an automated system for version bumping, tightly integrated with [Conventional Commits](https://www.conventionalcommits.org/) and GitHub Actions. This system analyzes actual code changes and commit messages to determine the appropriate semantic version increment.

### Pure Mathematical Version Detection

The automated release workflow (`version-bump.yml` in `.github/workflows/`) uses a **pure mathematical approach** to determine version bumps. The system calculates bonus points from various change types and applies them to determine the version bump:

-   **MAJOR Release**: Triggered when total bonus points ≥ 8. Includes breaking changes, API changes, security issues, and high-impact modifications.
-   **MINOR Release**: Triggered when total bonus points ≥ 4. Includes new features, CLI additions, significant improvements, and medium-impact changes.
-   **PATCH Release**: Triggered when total bonus points ≥ 0. **Any change gets at least a patch bump** - no minimum thresholds or extra rules.
-   **No Release**: Only occurs when there are truly no changes to analyze (e.g., single-commit repositories).

**No extra rules or minimum thresholds apply** - the system is purely mathematical based on bonus point calculations.

The system uses a LOC-based delta system to calculate the actual version increment, ensuring that even small changes get appropriate version bumps while larger changes get proportionally larger increments.

### Semantic Version Analyzer Tool

A dedicated script, `bin/semantic-version-analyzer.sh`, is used to analyze the Git history and suggest the next appropriate version bump. This tool is the core of our automated versioning system.

```bash
# Analyze changes since the last Git tag (default behavior)
./bin/semantic-version-analyzer.sh

# Analyze changes since a specific Git tag (e.g., v10.4.0)
./bin/semantic-version-analyzer.sh --since v10.4.0

# Show a detailed analysis, including file changes and commit messages
./bin/semantic-version-analyzer.sh --verbose

# Analyze changes since a specific date (e.g., all changes since January 1, 2025)
./bin/semantic-version-analyzer.sh --since-date 2025-01-01

# Get machine-readable JSON output
./bin/semantic-version-analyzer.sh --json

# Restrict analysis to specific paths
./bin/semantic-version-analyzer.sh --only-paths "src/**,include/**"

# Get only the suggestion (major/minor/patch/none)
./bin/semantic-version-analyzer.sh --suggest-only
```

#### What the Analyzer Checks

The `semantic-version-analyzer` performs a deep inspection of the codebase and Git history, with a particular focus on changes relevant to a command-line interface (CLI) tool:

1.  **File Changes**: Identifies added, modified, and deleted files across the repository (source code, tests, documentation, build scripts).
2.  **CLI Interface Analysis**: Specifically looks for patterns indicating changes to the CLI in C/C++ source files only:
    -   Introduction of new command-line options (suggests a `MINOR` bump).
    -   Removal of existing command-line options (suggests a `MAJOR` / breaking change).
    -   Enhancements to existing options (typically a `PATCH` or `MINOR` depending on impact).
3.  **Source Code Structure**: Detects new source files, test files, or significant refactorings that might imply new functionality or breaking changes.
4.  **Documentation Updates**: Notes new or significantly updated documentation files, which can sometimes correlate with new features.
5.  **Change Magnitude**: Quantifies the size of changes (e.g., lines added/deleted) to calculate LOC-based delta increments.
6.  **Universal Patch Detection**: **Any change** that doesn't qualify for major or minor bumps automatically triggers a patch bump, ensuring no changes are missed.
7.  **Security Analysis**: Detects security-related keywords and CVE references for appropriate bonus calculations.
8.  **Performance Analysis**: Identifies performance improvements and optimizations.

#### Configuration System

The analyzer supports both YAML configuration and environment variables:

**YAML Configuration (Recommended)**:
```bash
# Loads from config/versioning.yml
./bin/semantic-version-analyzer.sh
```

**Environment Variables (Fallback)**:
```bash
export VERSION_PATCH_DELTA="1*(1+LOC/250)"
export VERSION_MINOR_DELTA="5*(1+LOC/500)"
export VERSION_MAJOR_DELTA="10*(1+LOC/1000)"
./bin/semantic-version-analyzer.sh
```

#### Bonus System Categories

The current system includes 7 comprehensive bonus categories:

1. **Breaking Changes**: API breaking (+5), CLI breaking (+4), removed features (+3), etc.
2. **Security & Stability**: Security vulnerabilities (+5), CVE (+2), memory safety (+4), etc.
3. **Performance**: Performance improvements (+1-3), memory reduction (+2), build time (+1)
4. **Features**: New CLI commands (+2), new config options (+1), new file formats (+3)
5. **Code Quality**: Major refactors (+2), coverage improvements (+1), static analysis (+2)
6. **Infrastructure**: CI/CD changes (+1), build overhauls (+2), new platforms (+2)
7. **User Experience**: UI/UX improvements (+2), accessibility (+2), i18n (+3)

[↑ Back to top](#versioning-strategy)

## Manual Version Management

While automated versioning is the primary method, `next-version` provides options for manual control over version bumps and releases, typically via the GitHub Actions interface.

### Manually Triggering a Version Bump

This is useful for hotfixes, specific prereleases, or when you need to override the automatic detection:

1.  Navigate to your repository on GitHub.
2.  Go to the **Actions** tab.
3.  Select the workflow named **"Auto Version Bump with Semantic Release Notes"** from the left sidebar.
4.  Click the **"Run workflow"** dropdown button.
5.  In the form, you can:
    -   Choose the `Bump type` (e.g., `major`, `minor`, `patch`) or select `auto` to let the system detect it.
    -   Add `Custom release notes` that will be prepended to the automatically generated notes.
    -   Check `Prerelease` if you are creating a pre-release version (e.g., `v10.5.13-beta.1`).
6.  Click **"Run workflow"** to initiate the manual release process.

[↑ Back to top](#versioning-strategy)

## Version Management Tools

Several utility scripts are provided in the `bin/` directory to assist with version management tasks.

### `semantic-version-analyzer`

As described above, this script analyzes changes and suggests version bumps. It's a crucial tool for understanding the impact of your commits.

### `mathematical-version-bump`

The `bin/mathematical-version-bump.sh` script provides purely mathematical versioning - no manual bump types needed. The system automatically determines the appropriate version bump based on semantic analysis of changes.

```bash
# Automatically determine and apply version bump
./bin/mathematical-version-bump.sh --commit

# Dry run to see what would happen
./bin/mathematical-version-bump.sh --dry-run

# Set version directly
./bin/mathematical-version-bump.sh --set 10.5.13

# Analyze changes since specific tag
./bin/mathematical-version-bump.sh --since v10.4.0 --commit

# Print computed version without making changes
./bin/mathematical-version-bump.sh --print

# Create a signed tag
./bin/mathematical-version-bump.sh --commit --tag --signed-tag

# Push changes and tags to remote
./bin/mathematical-version-bump.sh --commit --tag --push --push-tags
```

### `tag-manager`

The `bin/tag-manager.sh` script provides functionalities for listing, creating, and cleaning up Git tags. This is essential for maintaining a tidy and accurate tag history.

```bash
# List all Git tags (sorted by version)
./bin/tag-manager.sh list

# Clean up old tags (interactively or by keeping a specific count)
./bin/tag-manager.sh cleanup [count]

# Create a new tag (use with caution, prefer automated releases)
./bin/tag-manager.sh create <version>

# Show detailed information about a specific tag
./bin/tag-manager.sh info <tag>
```

### Additional Version Tools

The project includes several specialized version management tools:

- **`version-calculator.sh`**: Core mathematical version calculation engine
- **`version-calculator-loc.sh`**: LOC-based delta calculation utilities
- **`version-config-loader.sh`**: YAML configuration loading and validation
- **`version-utils.sh`**: Common version utility functions
- **`version-validator.sh`**: Version format validation and testing

For more details on tag management, refer to the [Git Tag Management Guide](TAG_MANAGEMENT.md).

[↑ Back to top](#versioning-strategy)

## Release Process Integration

The versioning strategy is an integral part of the overall [Release Workflow](RELEASE_WORKFLOW.md). The typical flow is:

1.  **Develop and Commit**: Make changes and commit them using [Conventional Commit](https://www.conventionalcommits.org/) messages.
2.  **Push to `main`**: Push your changes to the `main` branch.
3.  **Automated Analysis**: The `version-bump.yml` GitHub Actions workflow is triggered. It uses `semantic-version-analyzer` to determine the appropriate version bump.
4.  **Version Update and Tagging**: If a bump is warranted, the `VERSION` file is updated, a new Git tag is created, and a GitHub Release is generated with automatically compiled release notes.
5.  **Verification**: Comprehensive CI/CD tests run on the new tag to ensure stability.

This automated process ensures that releases are consistent, well-documented, and accurately reflect the changes in the codebase.

[↑ Back to top](#versioning-strategy)

## Version History

`next-version` maintains a clear version history, accessible through Git tags and GitHub Releases.

### Recent Releases

You can view recent releases and their corresponding tags using Git commands:

```bash
# List all version tags, sorted by version (newest first)
git tag --sort=-version:refname

# Show recent commits along with their tags
git log --oneline --tags --decorate --max-count=10
```

For a more user-friendly view, refer to the [GitHub Releases page](https://github.com/eserlxl/next-version/releases) of the repository.

### Version Evolution

The project has evolved through several major versions, each marking significant milestones:

-   **v1.x**: Initial development, establishing core filtering and deduplication functionalities.
-   **v2.x**: Focused on performance improvements, introducing support for large files and stream processing.
-   **v3.x**: Expanded with advanced features, enhanced filtering capabilities, and a more comprehensive test suite.
-   **v4.x**: Implementation of robust semantic versioning, automated release workflows, and extensive CI/CD improvements.
-   **v10.x**: Current major version featuring the advanced LOC-based delta system with intelligent rollover logic, enhanced configuration management, and modular architecture.

[↑ Back to top](#versioning-strategy)

## Best Practices for Versioning

Adhering to these best practices ensures a smooth and accurate versioning process:

1.  **Consistent Commit Messages**: Always write clear, concise, and [Conventional Commit](https://www.conventionalcommits.org/)-compliant messages. This is the foundation for accurate automated version detection.
2.  **Understand Bump Types**: Be aware of what constitutes a MAJOR, MINOR, or PATCH change. This helps in writing appropriate commit messages and understanding the impact of your contributions.
3.  **Rely on Automation**: For most cases, let the automated GitHub Actions workflow handle version bumping and tagging. Avoid manually modifying the `VERSION` file or creating tags directly on `main`.
4.  **Review Release Notes**: Before a major release, review the automatically generated release notes to ensure they accurately capture all significant changes.
5.  **Prereleases for Major Changes**: For significant or breaking changes, consider creating prereleases (e.g., `v11.0.0-beta.1`) to allow for broader testing before a stable release.
6.  **Monitor LOC-Based Deltas**: Understand how the LOC-based delta system affects version increments and use the `--verbose` flag to see detailed calculations.
7.  **Configuration Management**: Use the YAML configuration system for consistent versioning behavior across different environments.
8.  **Test Version Calculations**: Use the `--dry-run` option with version bump tools to verify calculations before applying changes.
9.  **Understand Rollover Logic**: Be aware that the system uses mod 1000 rollover, so version 10.5.999 + 1 = 10.6.0.

[↑ Back to top](#versioning-strategy)

## Troubleshooting Versioning Issues

If you encounter problems related to versioning, consider the following troubleshooting steps:

### Common Issues

1.  **Version not detected or displayed as "unknown"**: Ensure the `VERSION` file exists in one of the expected locations and has read permissions. Verify the `next-version` executable is correctly built and linked.
2.  **Automatic release not triggered**: Check if your commit messages adhere to [Conventional Commits](https://www.conventionalcommits.org/) and if the changes meet the [automatic release detection thresholds](#automatic-release-detection-thresholds). Review the GitHub Actions workflow logs for any errors.
3.  **Incorrect version bump suggested/applied**: Manually run `semantic-version-analyzer --verbose` to understand why a particular bump was suggested. If you believe it's incorrect, you can manually trigger the workflow and override the bump type.
4.  **Tag conflicts or messy tag history**: Use the `tag-manager` script to list and clean up old or conflicting tags. Ensure you are not manually creating tags that conflict with the automated process.
5.  **LOC-based delta calculation issues**: Check the configuration in `config/versioning.yml` or environment variables. Ensure LOC divisors are greater than 0 to avoid division by zero errors.
6.  **Unexpected rollovers**: Review the current version numbers and LOC calculations. The system uses mod 1000 rollover logic, so version 10.5.999 + 1 = 10.6.0.
7.  **Configuration loading errors**: Verify that `config/versioning.yml` is properly formatted and accessible. Use `version-config-loader.sh` to validate the configuration.

### Getting Help

-   **GitHub Actions Logs**: The most valuable resource for troubleshooting automated versioning issues are the detailed logs of the `version-bump.yml` workflow runs.
-   **`semantic-version-analyzer` Output**: Use the verbose output of this tool to understand the analysis of your changes.
-   **Project Documentation**: Refer to the [FAQ](FAQ.md), [Developer Guide](DEVELOPER_GUIDE.md), [Release Workflow Guide](RELEASE_WORKFLOW.md), and [Git Tag Management Guide](TAG_MANAGEMENT.md) for more context.
-   **Version Algorithm**: See [VERSIONING_ALGORITHM.md](VERSIONING_ALGORITHM.md) for detailed technical implementation.
-   **GitHub Issues**: If you suspect a bug in the versioning tooling or the workflow itself, please [open an issue](https://github.com/eserlxl/next-version/issues) on the GitHub repository.

[↑ Back to top](#versioning-strategy)