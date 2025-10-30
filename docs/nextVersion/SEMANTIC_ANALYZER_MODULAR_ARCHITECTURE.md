# shellcheck disable=all
# Semantic Version Analyzer - Modular Architecture

This directory contains the refactored semantic version analyzer split into focused, modular components.

## Overview

The original monolithic semantic-version-analyzer has been refactored into smaller, focused binaries that each handle a specific aspect of version analysis. The main semantic-version-analyzer.sh now orchestrates these modular components.

## Components

### Core Components

#### 1. ref-resolver.sh
- **Purpose**: Resolves git references and determines base references for comparison
- **Key Features**:
  - Handles --since, --since-tag, --since-commit, --since-date options
  - Auto-detects base references (last tag, parent commit, etc.)
  - Manages merge-base detection for disjoint branches
  - Validates git references
  - **Usage**: ./bin/ref-resolver.sh --since v1.0.0 --target HEAD

#### 2. version-config-loader.sh
- **Purpose**: Loads and validates versioning configuration from YAML files and environment variables
- **Key Features**:
  - Loads configuration from config/versioning.yml
  - Falls back to environment variables
  - Validates configuration values
  - Supports multiple output formats (JSON, machine-readable)
- **Usage**: ./bin/version-config-loader.sh --validate-only

#### 3. file-change-analyzer.sh
- **Purpose**: Analyzes file changes and classifies them by type
- **Key Features**:
  - Tracks added, modified, deleted files
  - Classifies files as source, test, documentation
  - Calculates diff size
  - Handles rename/copy detection
- **Usage**: ./bin/file-change-analyzer.sh --base v1.0.0 --target HEAD

#### 4. cli-options-analyzer.sh
- **Purpose**: Detects and analyzes CLI option changes in C/C++ source files
- **Key Features**:
  - Extracts getopt/getopt_long options
  - Detects breaking CLI changes
  - Identifies API breaking changes
  - Manual CLI pattern detection
- **Usage**: ./bin/cli-options-analyzer.sh --base v1.0.0 --target HEAD

#### 5. security-keyword-analyzer.sh
- **Purpose**: Detects security-related keywords in commit messages and code changes
- **Key Features**:
  - Scans commit messages for security keywords
  - Detects CVE references
  - Identifies memory safety issues
  - Counts crash fixes
- **Usage**: ./bin/security-keyword-analyzer.sh --base v1.0.0 --target HEAD

#### 6. keyword-analyzer.sh
- **Purpose**: Detects breaking change keywords and other bonus indicators in code comments and commit messages
- **Key Features**:
  - Scans for breaking change keywords
  - Identifies feature additions
  - Detects bug fixes and improvements
  - Analyzes commit message patterns
- **Usage**: ./bin/keyword-analyzer.sh --base v1.0.0 --target HEAD

#### 7. version-calculator.sh
- **Purpose**: Calculates next version based on traditional semantic versioning rules
- **Key Features**:
  - Implements semantic versioning logic
  - Handles major, minor, patch bumps
  - Applies bonus points to version increments
  - Supports custom bump types
- **Usage**: ./bin/version-calculator.sh --current-version 1.2.3 --bump-type minor

#### 8. version-calculator-loc.sh
- **Purpose**: Calculates version bumps based on lines of code changes and semantic analysis
- **Key Features**:
  - Implements LOC-based delta formulas
  - Handles version rollover logic
  - Applies bonus points to version increments
  - Supports custom delta formulas
- **Usage**: ./bin/version-calculator-loc.sh --current-version 1.2.3 --loc 500

#### 9. mathematical-version-bump.sh
- **Purpose**: Purely mathematical versioning system - no manual bump types needed
- **Key Features**:
  - Automatic version calculation based on changes
  - Mathematical formulas for version increments
  - No human intervention required
  - Deterministic versioning
- **Usage**: ./bin/mathematical-version-bump.sh --since v1.0.0

#### 10. version-validator.sh
- **Purpose**: Validates version numbers and versioning rules
- **Key Features**:
  - Validates semantic version format
  - Checks version increment rules
  - Ensures version consistency
  - Provides validation feedback
- **Usage**: ./bin/version-validator.sh --version 1.2.3

#### 11. version-utils.sh
- **Purpose**: Common utility functions for version operations
- **Key Features**:
  - Version parsing and formatting
  - Common version operations
  - Shared helper functions
  - Version comparison utilities
- **Usage**: Source this file for utility functions

#### 12. tag-manager.sh
- **Purpose**: Manages git tags and version tags
- **Key Features**:
  - Creates and manages version tags
  - Handles tag validation
  - Manages tag history
  - Supports tag operations
- **Usage**: ./bin/tag-manager.sh --create-tag v1.2.3

#### 13. git-operations.sh
- **Purpose**: Common git operations for version analysis
- **Key Features**:
  - Git diff operations
  - Commit analysis
  - Branch management
  - Repository operations
- **Usage**: Source this file for git operations

### Orchestrator

#### 14. semantic-version-analyzer.sh
- **Purpose**: Orchestrates all modular components for complete version analysis (v2)
- **Key Features**:
  - Coordinates all analysis components
  - Calculates total bonus points
  - Determines version bump suggestions
  - Maintains compatibility with original interface
  - Supports both traditional and LOC-based versioning
- **Usage**: ./bin/semantic-version-analyzer.sh --since v1.0.0

## Benefits of Modular Architecture

### 1. **Maintainability**
- Each component has a single responsibility
- Easier to understand and modify individual components
- Reduced complexity per file

### 2. **Testability**
- Individual components can be tested in isolation
- Easier to write unit tests for specific functionality
- Better error isolation

### 3. **Reusability**
- Components can be used independently
- Other tools can leverage specific analysis capabilities
- Easier to integrate into CI/CD pipelines

### 4. **Performance**
- Components can be optimized independently
- Parallel execution possible for independent analyses
- Better resource utilization

### 5. **Extensibility**
- New analysis types can be added as separate components
- Existing components can be enhanced without affecting others
- Plugin-like architecture for custom analyzers

## Usage Examples

### Complete Analysis
```bash
# Full semantic version analysis
./bin/semantic-version-analyzer.sh --since v1.0.0 --verbose
```

### Component-based Analysis
```bash
# Get base reference
BASE_REF="$(./bin/ref-resolver.sh --since v1.0.0 --print-base)"

# Analyze file changes
./bin/file-change-analyzer.sh --base "$BASE_REF" --target HEAD --json

# Analyze CLI options
./bin/cli-options-analyzer.sh --base "$BASE_REF" --target HEAD --json

# Analyze security keywords
./bin/security-keyword-analyzer.sh --base "$BASE_REF" --target HEAD --json

# Calculate version using LOC-based system
./bin/version-calculator-loc.sh --current-version 1.2.3 --loc 500

# Calculate version using traditional system
./bin/version-calculator.sh --current-version 1.2.3 --bump-type minor
```

### Mathematical Versioning
```bash
# Pure mathematical versioning
./bin/mathematical-version-bump.sh --since v1.0.0
```

## Configuration

All components support the same configuration system:

1. **YAML Configuration**: config/versioning.yml
2. **Environment Variables**: Fallback configuration
3. **Command Line Options**: Override specific values

## Output Formats

Each component supports multiple output formats:

- **Human-readable**: Default detailed output
- **Machine-readable**: Key=value format for scripting
- **JSON**: Structured data for programmatic consumption

## Error Handling

Each component includes:
- Comprehensive input validation
- Clear error messages
- Appropriate exit codes
- Graceful fallbacks

## Versioning Systems

The modular architecture supports multiple versioning approaches:

### 1. **Traditional Semantic Versioning**
- Uses version-calculator.sh
- Manual bump type specification
- Standard major.minor.patch format

### 2. **LOC-based Versioning**
- Uses version-calculator-loc.sh
- Automatic calculation based on code changes
- Mathematical formulas for increments

### 3. **Pure Mathematical Versioning**
- Uses mathematical-version-bump.sh
- No manual intervention required
- Deterministic version calculation

## Future Enhancements

The modular architecture enables several future enhancements:

1. **Parallel Analysis**: Run independent analyses concurrently
2. **Custom Analyzers**: Add project-specific analysis components
3. **Plugin System**: Load analysis components dynamically
4. **Caching**: Cache analysis results for performance
5. **Distributed Analysis**: Run analyses across multiple machines
6. **Additional Analyzers**: Add more specialized analysis components

## Contributing

When adding new functionality:

1. **Create New Component**: Add a focused binary for new analysis type
2. **Update Orchestrator**: Integrate new component into semantic-version-analyzer.sh
3. **Maintain Compatibility**: Ensure existing interfaces remain functional
4. **Add Documentation**: Update this README with new component details
5. **Add Tests**: Include comprehensive tests for new components 