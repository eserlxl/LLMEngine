# Version Management Scripts - Modular Architecture

This directory contains modular version management scripts for the next-version project. The original monolithic `bump-version` script has been split into smaller, focused modules for better maintainability and development.

## Architecture Overview

The modular architecture consists of the following components:

### Core Scripts

- **`mathematical-version-bump.sh`** - Main mathematical version bumper that coordinates all modules
- **`semantic-version-analyzer.sh`** - Analyzes changes and determines appropriate bump type

### Analysis Modules

- **`cli-options-analyzer.sh`** - Analyzes CLI option changes and their impact
- **`file-change-analyzer.sh`** - Analyzes file changes and their classification
- **`keyword-analyzer.sh`** - Analyzes keywords in commit messages and code changes
- **`security-keyword-analyzer.sh`** - Analyzes security-related keywords and changes

### Utility Modules

- **`version-utils.sh`** - Common utilities and helper functions
  - Color management
  - Path resolution
  - File operations
  - Git utilities
  - Validation helpers
  - Error handling

- **`version-validator.sh`** - Version format validation and comparison
  - Semantic version validation
  - Version comparison
  - Pre-release handling
  - Version order validation

- **`version-calculator-loc.sh`** - LOC-based version calculation
  - Semantic analyzer integration
  - Rollover logic
  - Delta calculation
  - Configuration management

- **`version-calculator.sh`** - Basic version calculation utilities
  - Version increment logic
  - Basic arithmetic operations
  - Version string manipulation

- **`version-config-loader.sh`** - Configuration management
  - YAML configuration loading
  - Environment variable fallbacks
  - Configuration validation
  - Machine-readable output

### Git and Repository Modules

- **`git-operations.sh`** - Git commit, tag, and push operations
  - Commit creation
  - Tag management
  - Push operations
  - Dirty tree checking
  - Signing support

- **`tag-manager.sh`** - Advanced tag management
  - Tag creation and deletion
  - Tag validation
  - Tag synchronization
  - Tag prefix handling

- **`ref-resolver.sh`** - Git reference resolution
  - Commit hash resolution
  - Branch and tag resolution
  - Reference validation
  - Merge base detection

### Development and CI/CD Modules

- **`generate-ci-gpg-key.sh`** - GPG key generation for CI/CD
  - Key generation
  - Key configuration
  - CI integration setup

- **`sync_alpha.sh`** - Alpha version synchronization
  - Alpha version management
  - Development workflow integration

## Module Dependencies

```
mathematical-version-bump.sh
├── version-utils.sh
├── semantic-version-analyzer.sh
├── version-calculator-loc.sh
├── git-operations.sh
└── version-validator.sh

semantic-version-analyzer.sh
├── version-utils.sh
├── cli-options-analyzer.sh
├── file-change-analyzer.sh
├── keyword-analyzer.sh
├── security-keyword-analyzer.sh
├── ref-resolver.sh
└── version-config-loader.sh

version-calculator-loc.sh
├── version-utils.sh
├── version-config-loader.sh
└── semantic-version-analyzer.sh
```

## Usage

### Using the Mathematical Version Bumper

```bash
# Basic usage (purely mathematical)
./bin/mathematical-version-bump.sh --dry-run
./bin/mathematical-version-bump.sh --commit --tag
./bin/mathematical-version-bump.sh --set 1.0.0 --allow-prerelease

# Analyze changes since specific reference
./bin/mathematical-version-bump.sh --since v1.0.0 --commit
```

### Using Individual Modules

Each module can be used independently for specific tasks:

```bash
# Validate version format
./bin/version-validator.sh validate 1.0.0
./bin/version-validator.sh validate 1.0.0-rc.1 true

# Compare versions
./bin/version-validator.sh compare 1.0.0 1.0.1

# Calculate new version
./bin/version-calculator-loc.sh --current-version 1.0.0 --bump-type patch

# Check git operations
./bin/git-operations.sh check-dirty

# Analyze semantic changes
./bin/semantic-version-analyzer.sh --since v1.0.0 --verbose

# Load configuration
./bin/version-config-loader.sh --machine

# Analyze CLI options
./bin/cli-options-analyzer.sh --since v1.0.0
```

## Benefits of Modular Architecture

### Maintainability
- **Single Responsibility**: Each module has a focused purpose
- **Easier Testing**: Individual modules can be tested in isolation
- **Reduced Complexity**: Smaller, more manageable code units
- **Clear Dependencies**: Explicit module relationships

### Development
- **Parallel Development**: Multiple developers can work on different modules
- **Incremental Improvements**: Modules can be enhanced independently
- **Reusability**: Modules can be used by other scripts
- **Debugging**: Easier to isolate and fix issues

### Flexibility
- **Standalone Usage**: Modules can be used independently
- **Custom Integration**: Other scripts can use specific modules
- **Configuration**: Each module can have its own configuration
- **Extensibility**: New modules can be added easily

## Module Details

### version-utils.sh
Common utilities used across all modules:
- Color management with TTY detection
- Path resolution and validation
- File operations with safety checks
- Git repository utilities
- Error handling and logging
- Validation helpers

### version-validator.sh
Handles all version-related validation:
- Semantic version format validation
- Version comparison logic
- Pre-release format support
- Version order validation
- Tag prefix handling

### version-calculator-loc.sh
LOC-based version calculation with semantic analysis:
- Integration with semantic-version-analyzer
- Rollover logic for version limits
- Delta calculation based on code changes
- Configuration via environment variables
- Fallback to default deltas

### version-calculator.sh
Basic version calculation utilities:
- Version increment operations
- Arithmetic calculations
- String manipulation
- Basic validation

### version-config-loader.sh
Configuration management and loading:
- YAML configuration file parsing
- Environment variable fallbacks
- Configuration validation
- Machine-readable output formats
- Default value management

### git-operations.sh
Comprehensive git operations management:
- Commit creation with proper messages
- Tag creation (lightweight, annotated, signed)
- Push operations for branches and tags
- Dirty tree validation
- Signing key validation
- Summary generation

### tag-manager.sh
Advanced tag management capabilities:
- Tag creation and deletion
- Tag validation and verification
- Tag synchronization across remotes
- Tag prefix and naming conventions
- Tag metadata management

### ref-resolver.sh
Git reference resolution and validation:
- Commit hash resolution
- Branch and tag reference handling
- Reference validation and verification
- Merge base detection
- Reference comparison utilities

### semantic-version-analyzer.sh
Comprehensive semantic analysis:
- Change analysis since specific references
- Integration with all analysis modules
- Machine-readable output formats
- Configurable analysis scope
- Detailed progress reporting

### cli-options-analyzer.sh
CLI option change analysis:
- Option addition/removal detection
- Option modification analysis
- Breaking change identification
- CLI compatibility assessment

### file-change-analyzer.sh
File change classification and analysis:
- File type classification
- Change impact assessment
- Source vs test vs documentation analysis
- File importance weighting

### keyword-analyzer.sh
Keyword-based change analysis:
- Commit message keyword extraction
- Code comment keyword analysis
- Change classification based on keywords
- Semantic meaning extraction

### security-keyword-analyzer.sh
Security-focused analysis:
- Security-related keyword detection
- Vulnerability indicator analysis
- Security change impact assessment
- Security bonus calculation

### generate-ci-gpg-key.sh
CI/CD GPG key management:
- Automated key generation
- CI environment configuration
- Key export and import utilities
- Signing setup automation

### sync_alpha.sh
Alpha version synchronization:
- Development workflow integration
- Alpha version management
- Pre-release coordination
- Development branch synchronization

## Migration Guide

### For Users
No changes required - the modular script maintains full backward compatibility with the original `bump-version` script.

### For Developers
- Use `mathematical-version-bump.sh` for new development
- Individual modules can be used for specific tasks
- New features should be added to appropriate modules
- Testing can be done on individual modules

### For CI/CD
- Update workflows to use `mathematical-version-bump.sh` if desired
- Individual modules can be used for specific CI tasks
- No changes required for existing workflows

## Testing

Each module includes standalone usage for testing:

```bash
# Test version validation
./bin/version-validator.sh validate 1.0.0

# Test version calculation
./bin/version-calculator-loc.sh --current-version 1.0.0 --bump-type patch

# Test git operations
./bin/git-operations.sh check-dirty

# Test semantic analysis
./bin/semantic-version-analyzer.sh --since v1.0.0 --verbose

# Test configuration loading
./bin/version-config-loader.sh --validate-only
```

## Future Enhancements

The modular architecture enables several future enhancements:

1. **Plugin System**: Additional modules for different versioning strategies
2. **Configuration Management**: Centralized configuration for all modules
3. **API Integration**: Modules for external service integration
4. **Advanced Validation**: Enhanced validation rules and custom validators
5. **Multi-format Support**: Support for different version file formats
6. **Audit Trail**: Comprehensive logging and audit capabilities
7. **Performance Optimization**: Parallel processing for large repositories
8. **Machine Learning**: AI-powered change classification

## Contributing

When contributing to the version management system:

1. **Identify the appropriate module** for your changes
2. **Maintain module boundaries** - don't add unrelated functionality
3. **Update documentation** for any new features
4. **Add tests** for new functionality
5. **Ensure backward compatibility** when possible
6. **Follow the existing patterns** for consistency
7. **Update dependencies** when adding new module relationships

## License

All scripts are licensed under the GNU General Public License v3.0 or later, same as the main project. 