# LOC-Based Delta System

## Overview

The LOC-based delta system is an advanced versioning mechanism that **always increases only the last identifier (patch)** with a delta calculated based on the magnitude of code changes. This system prevents version number inflation while maintaining semantic meaning and provides intelligent rollover logic with performance optimizations.

## Problem Solved

For projects with rapid iteration cycles, traditional semantic versioning can lead to version number inflation:
- Small bug fixes increment versions unnecessarily
- Large changes get the same increment as small ones
- Version numbers grow too quickly
- **New Problem**: Restrictive thresholds and extra rules prevent pure mathematical logic from determining version bumps
- **Performance Issue**: Expensive LOC calculations for every change

## How It Works

### Core Concept
The new versioning system **always increases only the last identifier (patch)** with a delta calculated based on Lines of Code (LOC) changed plus bonus additions for specific types of changes. **Every change now results in at least a patch bump**, with the LOC-based system determining the actual increment amount:

```bash
# Base delta from LOC
PATCH: 1 * (1 + LOC/250)  # Small changes get small increments
MINOR: 5 * (1 + LOC/500)  # Medium changes get medium increments  
MAJOR: 10 * (1 + LOC/1000) # Large changes get large increments

# Enhanced bonus system with 7 categories
+ Breaking changes: API (+5), CLI (+4), Removed features (+3)
+ Security & stability: Security vuln (+5), CVE (+2), Memory safety (+4)
+ Performance: 50%+ perf (+3), 20-50% perf (+2), Memory reduction (+2)
+ Features: New CLI (+2), New config (+1), New files (+1)
+ Code quality: Major refactor (+2), Coverage (+1), Static analysis (+2)
+ Infrastructure: CI/CD (+1), Build overhaul (+2), New platform (+2)
+ User experience: UI/UX (+2), Accessibility (+2), i18n (+3)
```

### Key Features

1. **Always Increase Only the Last Identifier**: All version changes increment only the patch version (the last number)
2. **LOC-Based Delta Calculation**: The increment amount is calculated based on Lines of Code (LOC) changed plus bonus additions
3. **Rollover Logic**: Uses mod 1000 for patch and minor version limits with automatic rollover
4. **Enhanced Reason Format**: Includes LOC value and version type in analysis output
5. **Universal Patch Detection**: Every change results in at least a patch bump
6. **Early Exit Optimization**: Skips expensive LOC calculation when bonus threshold is met
7. **Multiplier System**: Applies multipliers for critical scenarios and scope
8. **Penalty System**: Reduces bonus for quality issues

### Enhanced Bonus System
The system automatically detects and applies bonuses across 7 comprehensive categories:

#### Breaking Changes (High Impact)
- **API breaking changes**: +5 (function signature changes, removed prototypes)
- **CLI breaking changes**: +4 (CLI option removals)
- **Removed features**: +3 (deprecated feature removals)
- **Database schema changes**: +4 (schema modifications)
- **Config format changes**: +3 (configuration file format changes)
- **Plugin API changes**: +3 (plugin interface modifications)
- **Deprecated removal**: +1 (removal of deprecated features)

#### Security & Stability (Critical)
- **Security vulnerabilities**: +5 (critical security fixes)
- **CVE fixes**: +2 (Common Vulnerabilities and Exposures)
- **Memory safety**: +4 (memory leak fixes, buffer overflows)
- **Race conditions**: +3 (concurrency fixes)
- **Resource leaks**: +2 (file handle, memory leaks)
- **Crash fixes**: +3 (application stability improvements)
- **Data corruption**: +4 (data integrity fixes)

#### Performance & Optimization
- **50%+ performance improvement**: +3 (major performance gains)
- **20-50% performance improvement**: +2 (moderate performance gains)
- **Memory reduction**: +2 (meets threshold percentage)
- **Build time improvement**: +1 (meets threshold percentage)
- **Runtime optimization**: +1 (general performance improvements)

#### Feature Additions (Medium Impact)
- **New CLI commands**: +2 (new command-line interfaces)
- **New config options**: +1 (new configuration parameters)
- **New file formats**: +3 (new supported file types)
- **New API endpoints**: +2 (new API functionality)
- **New plugin systems**: +4 (extensibility improvements)
- **New output formats**: +2 (new output options)
- **New source files**: +1 (new source code files)

#### Code Quality & Maintenance
- **Major refactoring**: +2 (significant code restructuring)
- **Coverage improvements**: +1 (10%+ test coverage increase)
- **Static analysis**: +2 (code quality improvements)
- **New test suites**: +1 (comprehensive testing additions)
- **Documentation overhaul**: +1 (major documentation updates)
- **Code style improvements**: +1 (coding standard compliance)

#### Infrastructure & Tooling
- **CI/CD changes**: +1 (continuous integration improvements)
- **Build system overhaul**: +2 (major build system changes)
- **Major dependencies**: +1 (significant dependency updates)
- **New platform support**: +2 (cross-platform compatibility)
- **Containerization**: +2 (Docker/container support)
- **Cloud integration**: +3 (cloud service integration)

#### User Experience
- **UI/UX improvements**: +2 (user interface enhancements)
- **Accessibility**: +2 (accessibility compliance)
- **Internationalization**: +3 (multi-language support)
- **Error messages**: +1 (improved error handling)
- **User documentation**: +1 (user guide improvements)

### Multiplier System
The system applies multipliers for critical scenarios and scope:

#### Critical Multipliers
- **Zero-day vulnerabilities**: 2.0x (critical security issues)
- **Production outages**: 2.0x (service disruption fixes)
- **Compliance requirements**: 1.5x (regulatory compliance)

#### Scope Multipliers
- **Customer requests**: 1.2x (customer-driven changes)
- **Cross-platform changes**: 1.3x (multi-platform support)
- **Backward compatibility**: 1.2x (compatibility maintenance)
- **Migration tools**: 1.1x (upgrade assistance)

### Penalty System
The system applies penalties for quality issues (additive, can result in negative bonus):
- **No migration path**: -2 (breaking changes without migration)
- **Incomplete documentation**: -1 (missing documentation)
- **Missing tests**: -1 (untested changes)
- **Performance regression**: -2 (performance degradation)

### Early Exit Optimization
The system includes performance optimizations:
- **Bonus threshold**: If bonus >= 8, skips expensive LOC calculation
- **Change type detection**: Automatically determines major/minor/patch
- **Configuration validation**: Prevents division by zero errors
- **Efficient parsing**: Uses deterministic git diff commands

### Pure Mathematical Version Detection
**Every change now results in at least a patch bump**, determined purely by mathematical bonus point calculations. The old restrictive thresholds and extra rules have been completely removed:

- **Total bonus points ≥ 0** triggers a patch bump (any change gets at least patch)
- **Total bonus points ≥ 4** triggers a minor bump
- **Total bonus points ≥ 8** triggers a major bump
- The LOC-based delta system then calculates the actual increment amount
- **No extra rules, minimum thresholds, or file count requirements apply**

### New Rollover System
The system implements intelligent rollover logic:

- **Patch rollover**: When patch + delta >= 1000, apply mod 1000 and increment minor
- **Minor rollover**: When minor + 1 >= 1000, apply mod 1000 and increment major
- **Example**: 10.5.995 + 6 = 10.6.1 (patch rollover)
- **Example**: 10.999.995 + 6 = 11.0.1 (minor rollover)

### Enhanced Reason Format
The system now provides enhanced analysis output that includes:
- **LOC value**: The actual lines of code changed
- **Version type**: MAJOR, MINOR, or PATCH
- **Bonus breakdown**: Detailed bonus calculations by category
- **Multiplier application**: Applied multipliers and their effects
- **Penalty application**: Applied penalties and their effects
- **Example**: "cli_added (LOC: 200, MINOR, Bonus: +5, Multiplier: 1.2x)"

### Examples

#### Small Change (50 LOC) - No Bonuses
```bash
Base PATCH: 1 * (1 + 50/250) = 1.2 → 1
Base MINOR: 5 * (1 + 50/500) = 5.5 → 5
Base MAJOR: 10 * (1 + 50/1000) = 10.5 → 10

Total bonus: 0 points
Decision: PATCH (bonus ≥ 0)
Result: 10.5.12 → 10.5.13 (patch)
```

#### Medium Change (500 LOC) with CLI Additions
```bash
Base PATCH: 1 * (1 + 500/250) = 3
Bonus: CLI changes (+2) + Added options (+1) = +3
Total bonus: 3 points
Decision: PATCH (bonus < 4)
Final PATCH: 3 + 3 = 6

Result: 10.5.12 → 10.5.18 (patch)
```

#### Large Change (2000 LOC) with Breaking Changes
```bash
Base MAJOR: 10 * (1 + 2000/1000) = 30
Bonus: Breaking CLI (+2) + API breaking (+3) + Removed options (+2) = +7
Total bonus: 7 points
Decision: MINOR (bonus ≥ 4 but < 8)
Final MINOR: 5 * (1 + 2000/500) + 7 = 25 + 7 = 32

Result: 10.5.12 → 10.5.44 (patch with minor-level delta)
```

#### Security Fix (100 LOC) with Security Keywords
```bash
Base PATCH: 1 * (1 + 100/250) = 1.4 → 1
Bonus: Security keywords (3 × +2) = +6
Total bonus: 6 points
Decision: MINOR (bonus ≥ 4)
Final MINOR: 5 * (1 + 100/500) + 6 = 6 + 6 = 12

Result: 10.5.12 → 10.5.24 (patch with minor-level delta)
```

#### Critical Security Fix with Multiplier
```bash
Base PATCH: 1 * (1 + 200/250) = 1.8 → 1
Bonus: Security vulnerability (+5) + CVE (+2) = +7
Multiplier: Zero-day (2.0x) = 14
Total bonus: 14 points
Decision: MAJOR (bonus ≥ 8)
Final MAJOR: 10 * (1 + 200/1000) + 14 = 12 + 14 = 26

Result: 10.5.12 → 10.5.38 (patch with major-level delta)
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

## Configuration

### YAML Configuration (Recommended)

The system uses `config/versioning.yml` for comprehensive configuration:

```yaml
# Base deltas for different change types
base_deltas:
  patch: "1"
  minor: "5"
  major: "10"

# LOC cap and rollover configuration
limits:
  loc_cap: 10000
  rollover: 1000  # Three-digit rollover for better version management

# LOC divisors for different change types
loc_divisors:
  major: 1000
  minor: 500
  patch: 250

# Enhanced bonus system - 7 categories with point values
bonuses:
  breaking_changes:
    api_breaking: 5
    cli_breaking: 4
    removed_features: 3
    database_schema: 4
    config_format: 3
    plugin_api: 3
    deprecated_removal: 1
  
  security_stability:
    security_vuln: 5
    cve: 2
    memory_safety: 4
    race_condition: 3
    resource_leak: 2
    crash_fix: 3
    data_corruption: 4
  
  performance:
    perf_50_plus: 3
    perf_20_50: 2
    memory_reduction_pct: 2
    build_time_pct: 1
    runtime_opt: 1
  
  features:
    new_cli_command: 2
    new_config_option: 1
    new_file_format: 3
    new_api_endpoint: 2
    new_plugin_system: 4
    new_output_format: 2
    new_source_file: 1
  
  code_quality:
    major_refactor: 2
    coverage_10_plus: 1
    static_analysis: 2
    new_test_suite: 1
    doc_overhaul: 1
    code_style: 1
  
  infrastructure:
    cicd_changes: 1
    build_overhaul: 2
    major_deps: 1
    new_platform: 2
    containerization: 2
    cloud_integration: 3
  
  user_experience:
    ui_ux_improvement: 2
    accessibility: 2
    i18n: 3
    error_messages: 1
    user_docs: 1

# Multiplier system for critical scenarios
multipliers:
  critical:
    zero_day: 2.0
    production_outage: 2.0
    compliance: 1.5
  
  scope:
    customer_request: 1.2
    cross_platform: 1.3
    backward_compat: 1.2
    migration_tools: 1.1

# Penalty system for quality issues
penalties:
  no_migration_path: -2
  incomplete_docs: -1
  missing_tests: -1
  perf_regression: -2

# Decision tree thresholds - PURELY MATHEMATICAL
# These thresholds determine version bump type based on total bonus points
# No other rules or conditions apply - pure math logic only
thresholds:
  major_bonus: 8    # Total bonus >= 8 = MAJOR
  minor_bonus: 4    # Total bonus >= 4 = MINOR  
  patch_bonus: 0    # Total bonus >= 0 = PATCH (any change gets at least patch)

# Pattern matching configuration
patterns:
  performance:
    memory_reduction_threshold: 30
    build_time_threshold: 50
    perf_50_threshold: 50
  
  early_exit:
    bonus_threshold: 8
    change_type: "major"
```

### Environment Variables (Fallback)
```bash
# Enable the new versioning system

# Delta formulas
VERSION_PATCH_DELTA="1*(1+LOC/250)"
VERSION_MINOR_DELTA="5*(1+LOC/500)"
VERSION_MAJOR_DELTA="10*(1+LOC/1000)"

# Rollover limits
VERSION_PATCH_LIMIT=1000
VERSION_MINOR_LIMIT=1000

# Enhanced bonus values
VERSION_BREAKING_CLI_BONUS=2
VERSION_API_BREAKING_BONUS=3
VERSION_REMOVED_OPTION_BONUS=1
VERSION_CLI_CHANGES_BONUS=2
VERSION_MANUAL_CLI_BONUS=1
VERSION_NEW_SOURCE_BONUS=1
VERSION_NEW_TEST_BONUS=1
VERSION_NEW_DOC_BONUS=1
VERSION_ADDED_OPTION_BONUS=1
VERSION_SECURITY_BONUS=2

# PURELY MATHEMATICAL VERSIONING SYSTEM
# All version bump decisions are based on bonus point calculations
# No minimum thresholds or extra rules - pure math logic only
```

### Default Values
- `VERSION_PATCH_LIMIT=1000`
- `VERSION_MINOR_LIMIT=1000`
- `VERSION_PATCH_DELTA="1*(1+LOC/250)"`
- `VERSION_MINOR_DELTA="5*(1+LOC/500)"`
- `VERSION_MAJOR_DELTA="10*(1+LOC/1000)"`
- `VERSION_BREAKING_CLI_BONUS=2`
- `VERSION_API_BREAKING_BONUS=3`
- `VERSION_REMOVED_OPTION_BONUS=1`
- `VERSION_CLI_CHANGES_BONUS=2`
- `VERSION_MANUAL_CLI_BONUS=1`
- `VERSION_NEW_SOURCE_BONUS=1`
- `VERSION_NEW_TEST_BONUS=1`
- `VERSION_NEW_DOC_BONUS=1`
- `VERSION_ADDED_OPTION_BONUS=1`
- `VERSION_SECURITY_BONUS=2`

**PURELY MATHEMATICAL VERSIONING SYSTEM**
- All version bump decisions are based on bonus point calculations
- No minimum thresholds or extra rules - pure math logic only
- `MAJOR_BONUS_THRESHOLD=8` (total bonus ≥ 8 = MAJOR)
- `MINOR_BONUS_THRESHOLD=4` (total bonus ≥ 4 = MINOR)
- `PATCH_BONUS_THRESHOLD=0` (total bonus ≥ 0 = PATCH)

## Usage

### System Status
The LOC-based delta system is **always enabled** by default. No configuration is required.

### Normal Version Bumping
```bash
# The system automatically calculates deltas based on LOC
# All bumps now increment only the patch version with calculated delta
./bin/bump-version.sh patch --commit
./bin/bump-version.sh minor --commit
./bin/bump-version.sh major --commit
```

### View Delta Calculations
```bash
# See calculated deltas in verbose mode
./bin/semantic-version-analyzer.sh --verbose

# Get JSON output with delta information
./bin/semantic-version-analyzer.sh --json

# Get machine-readable output
./bin/semantic-version-analyzer.sh --machine
```

### Custom Formulas
```bash
# Use different scaling factors
export VERSION_PATCH_DELTA="1*(1+LOC/100)"   # More aggressive
export VERSION_MINOR_DELTA="3*(1+LOC/300)"   # Less aggressive
export VERSION_MAJOR_DELTA="5*(1+LOC/500)"   # Conservative
```

## Integration with Semantic Analyzer

The system integrates seamlessly with the existing semantic version analyzer:

### What It Detects
- **Breaking changes**: API changes, CLI removals, signature changes
- **Feature additions**: New CLI options, new files, new functionality
- **Change magnitude**: Lines of code changed, file counts
- **Security issues**: Security-related keywords and patterns
- **Performance improvements**: Performance-related changes and thresholds
- **Code quality**: Refactoring, testing, documentation improvements
- **Infrastructure**: CI/CD, build system, platform changes
- **User experience**: UI/UX, accessibility, internationalization

### Enhanced Analysis
When enabled, the analyzer provides:
- LOC-based delta calculations
- Rollover warnings
- Detailed change analysis with bonus breakdown
- JSON output with delta information
- Enhanced reason format with LOC and version type
- Early exit optimization for performance
- Multiplier and penalty application

### Early Exit Optimization
The system includes performance optimizations:
- If bonus threshold is met, skips expensive LOC calculation
- Configuration validation prevents division by zero errors
- Efficient parsing with deterministic git diff commands
- Automatic change type detection based on bonus values

## Benefits

### For Rapid Iteration
- **Pure mathematical logic**: All decisions based on bonus point calculations
- **Proportional versioning**: Bigger changes = bigger version jumps
- **Prevents inflation**: Version numbers stay manageable
- **Predictable progression**: Clear rollover rules with mod 1000
- **Maintains semantics**: Still follows semver principles
- **Always increases patch**: Consistent behavior across all change types
- **Performance optimized**: Early exit for high-impact changes
- **No extra rules**: Eliminates arbitrary thresholds and restrictions

### For Different Project Sizes
- **Small projects**: Small changes get small increments
- **Large projects**: Large changes get appropriate increments
- **Configurable**: Can be tuned per project needs
- **Rollover protection**: Prevents version number overflow
- **Quality incentives**: Penalties encourage good practices

### For Critical Changes
- **Security focus**: High bonuses for security fixes
- **Critical multipliers**: 2x for zero-day and production issues
- **Compliance support**: 1.5x for regulatory requirements
- **Customer alignment**: 1.2x for customer-driven changes

## Testing

Run the comprehensive test suite to see the system in action:

```bash
# Test the new versioning system
./test-workflows/core-tests/test_loc_delta_system.sh

# Test rollover logic
./test-workflows/core-tests/test_rollover_logic.sh

# Test LOC delta system
./test-workflows/core-tests/test_loc_delta_system_comprehensive.sh

# Test bump-version integration
./test-workflows/core-tests/test_bump_version_loc_delta.sh
```

This demonstrates:
- Small, medium, and large changes
- Delta calculations with new formulas
- Rollover scenarios with mod 1000 logic
- Configuration options
- Enhanced reason format
- Integration with semantic analyzer
- Early exit optimization
- Multiplier and penalty systems

## Migration

### From Traditional Versioning
2. Continue using normal bump commands
3. The system automatically calculates appropriate deltas
4. All changes now increment only the patch version

### Backward Compatibility
- The system is enabled by default
- Traditional versioning still works when disabled
- Can be enabled/disabled per project
- Existing workflows continue to function

## Advanced Configuration

### Custom Scaling
```bash
# Logarithmic scaling
VERSION_PATCH_DELTA="1 + log(1+LOC/50)"

# Exponential scaling (for very large changes)
VERSION_MAJOR_DELTA="10 * (1 + LOC/1000)^0.5"

# Different limits for different project types
VERSION_PATCH_LIMIT=50   # For small projects
VERSION_MINOR_LIMIT=25   # For rapid iteration
```

### Impact-Based Weighting
```bash
# Weight different types of changes
VERSION_SOURCE_WEIGHT=1.0    # Source code changes
VERSION_TEST_WEIGHT=0.5      # Test changes
VERSION_DOC_WEIGHT=0.3       # Documentation changes
```

### YAML Configuration Examples
```yaml
# Conservative versioning for stable projects
base_deltas:
  patch: "0.5"
  minor: "2"
  major: "5"

# Aggressive versioning for rapid development
base_deltas:
  patch: "2"
  minor: "10"
  major: "20"

# Custom bonus values for specific project needs
bonuses:
  breaking_changes:
    api_breaking: 5  # Higher penalty for API breaks
    cli_breaking: 3
  security_stability:
    security_vuln: 10  # Critical security issues
    cve: 5
```

## Troubleshooting

### Common Issues

#### Delta Not Calculating
- Check that semantic analyzer is executable
- Verify LOC data is available
- Check YAML configuration syntax

#### Unexpected Rollovers
- Check current version numbers
- Review LOC calculations
- Adjust limits if needed
- Remember: all changes increment only patch version

#### Formula Errors
- Ensure formulas use valid syntax
- Check for division by zero
- Validate numeric inputs
- Verify YAML configuration

#### Early Exit Issues
- Check bonus threshold configuration
- Verify early exit threshold is appropriate
- Review bonus calculations
- Ensure change type detection is working

### Debug Mode
```bash
# Enable verbose output
./bin/semantic-version-analyzer.sh --verbose

# Check JSON output
./bin/semantic-version-analyzer.sh --json | jq '.loc_delta'

# Test rollover logic directly
./test-workflows/core-tests/test_rollover_logic.sh

# Validate configuration
./bin/version-config-loader.sh --machine
```

### Configuration Validation
The system validates configuration at load time:
- LOC divisors must be > 0
- Multipliers must be numeric
- YAML syntax must be valid
- Required fields must be present
- Bonus values must be numeric
- Penalty values must be negative

## Future Enhancements

### Planned Features
- **Impact-based weighting**: Different weights for different change types
- **Time-based factors**: Consider time since last release
- **Complexity metrics**: Beyond just LOC
- **Project-specific tuning**: Automatic configuration based on project size
- **Enhanced rollover visualization**: Better display of rollover scenarios
- **Machine learning integration**: Learn from historical versioning decisions
- **Real-time analysis**: Continuous monitoring of changes
- **Integration with CI/CD**: Automated version bumping in pipelines

### Contributing
The system is designed to be extensible. New delta formulas and detection methods can be easily added. The new versioning system maintains backward compatibility while providing enhanced functionality.

### Performance Considerations
- Early exit optimization reduces computation time
- Caching of LOC calculations for repeated analysis
- Efficient git diff parsing with minimal memory usage
- Parallel processing for large repositories
- Configuration validation prevents runtime errors
- Deterministic git commands ensure consistent results 