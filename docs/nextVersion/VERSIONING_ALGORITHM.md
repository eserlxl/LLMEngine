# Versioning Algorithm Implementation

This document explains how the next-version versioning system implements its mathematical algorithm for version calculation.

## Core Algorithm Overview

The versioning system uses a two-stage approach:
1. **Semantic Analysis**: Determines the suggested version bump type (major/minor/patch) based on bonus points
2. **Version Calculation**: Applies LOC-based deltas and bonus multipliers to calculate the final version

## Stage 1: Semantic Version Analysis

### Bonus Point Calculation

The semantic analyzer calculates total bonus points from various sources:

- **CLI Breaking Changes**: +2 points (configurable)
- **API Breaking Changes**: +3 points (configurable) 
- **General Breaking Changes**: +3 points (configurable)
- **Security Keywords**: +2 points per security keyword (configurable)

### Suggestion Logic

The system uses bonus thresholds to determine the suggested version bump:

```bash
if TOTAL_BONUS >= major_threshold:    suggest "major"
elif TOTAL_BONUS >= minor_threshold:  suggest "minor" 
elif TOTAL_BONUS > patch_threshold:   suggest "patch"
else:                                 suggest "none"
```

**Note**: The bonus system determines the **suggestion** but does not prevent LOC-based calculations in the version calculator.

## Stage 2: Version Calculation Algorithm

### Base Delta Calculation

The version calculator uses simplified formulas for base deltas:

- **PATCH**: `1 + round(LOC/250)`
- **MINOR**: `5 + round(LOC/100)`
- **MAJOR**: `10 + round(LOC/100)`

Where `round()` uses nearest-integer rounding: `(n + divisor/2) / divisor`

### Bonus Multiplier System

Bonus points are multiplied by a LOC-based factor:

```
bonus_multiplier = 1 + LOC/loc_divisor
```

Where `loc_divisor` depends on the bump type:
- **PATCH**: 250
- **MINOR**: 500  
- **MAJOR**: 1000

### Total Delta Calculation

```
total_bonus = BONUS + round(BONUS * LOC / loc_divisor)
total_delta = base_delta + total_bonus
```

The system ensures `total_delta >= 1` as a minimum.

### Mathematical Rollover System

The system uses `MAIN_VERSION_MOD = 1000` (configurable) and implements rollover logic:

For version `x.y.z` and total delta `delta_z`:

```
z_new = (z + delta_z) % MAIN_VERSION_MOD
delta_y = floor((z + delta_z) / MAIN_VERSION_MOD)
y_new = (y + delta_y) % MAIN_VERSION_MOD  
delta_x = floor((y + delta_y) / MAIN_VERSION_MOD)
x_new = x + delta_x
```

## Implementation Details

### Version Calculator (`bin/version-calculator.sh`)

Key functions:
- `calc_base_delta()`: Implements the base delta formulas
- `round_div()`: Provides nearest-integer rounding
- `fmt_fixed2_from_int100()`: Formats bonus multiplier as 2-decimal string

### Semantic Version Analyzer (`bin/semantic-version-analyzer.sh`)

Key features:
- Calculates total bonus from multiple sources
- Uses configurable thresholds for suggestions
- Passes bonus to version calculator for final calculation
- Provides detailed output in multiple formats (JSON, machine-readable, human-readable)

## Example Calculations

### Example 1: Patch with Low LOC
- Current: 1.2.3
- LOC: 100, Bonus: 5, Type: patch
- Base delta: 1 + round(100/250) = 1 + 0 = 1
- Multiplier: 1 + 100/250 = 1.40
- Total bonus: 5 + round(5*100/250) = 5 + 2 = 7
- Total delta: 1 + 7 = 8
- Result: 1.2.11

### Example 2: Minor with High LOC
- Current: 1.2.3  
- LOC: 500, Bonus: 10, Type: minor
- Base delta: 5 + round(500/100) = 5 + 5 = 10
- Multiplier: 1 + 500/500 = 2.00
- Total bonus: 10 + round(10*500/500) = 10 + 10 = 20
- Total delta: 10 + 20 = 30
- Result: 1.2.33

### Example 3: Rollover Case
- Current: 1.2.995
- LOC: 100, Bonus: 10, Type: patch
- Base delta: 1 + round(100/250) = 1 + 0 = 1
- Multiplier: 1 + 100/250 = 1.40
- Total bonus: 10 + round(10*100/250) = 10 + 4 = 14
- Total delta: 1 + 14 = 15
- New patch: 995 + 15 = 1010
- Rollover: 1010 → 1.3.10

## Configuration

The system supports configuration through `config/versioning.yml`:

- `VERSION_BREAKING_CLI_BONUS`: Bonus for CLI breaking changes
- `VERSION_API_BREAKING_BONUS`: Bonus for API breaking changes
- `VERSION_SECURITY_BONUS`: Bonus multiplier for security keywords
- `VERSION_MAJOR_THRESHOLD`: Threshold for major version suggestions
- `VERSION_MINOR_THRESHOLD`: Threshold for minor version suggestions
- `VERSION_PATCH_THRESHOLD`: Threshold for patch version suggestions

## Output Formats

The system provides multiple output formats:

### Human-Readable
```
Current version: 1.2.3
Suggested bump: MINOR
Next version: 1.2.33

Calculation Details:
  Lines of code: 500
  Base bonus: 10
  Base delta: 10
  Bonus multiplier: 2.00
  Total bonus: 20
  Total delta: 30
  Main version mod: 1000
  LOC divisor: 500

Reason: LOC=500, MINOR update, base_delta=10, bonus=10*2.00=20, total_delta=30
```

### Machine-Readable
```
CURRENT_VERSION=1.2.3
SUGGESTION=minor
NEXT_VERSION=1.2.33
LOC=500
BONUS=10
BASE_DELTA=10
BONUS_MULTIPLIER=2.00
TOTAL_BONUS=20
TOTAL_DELTA=30
MAIN_VERSION_MOD=1000
LOC_DIVISOR=500
REASON=LOC=500, MINOR update, base_delta=10, bonus=10*2.00=20, total_delta=30
```

### JSON
```json
{
  "current_version": "1.2.3",
  "bump_type": "minor",
  "next_version": "1.2.33",
  "loc": 500,
  "bonus": 10,
  "base_delta": 10,
  "bonus_multiplier": "2.00",
  "total_bonus": 20,
  "total_delta": 30,
  "main_version_mod": 1000,
  "loc_divisor": 500,
  "reason": "LOC=500, MINOR update, base_delta=10, bonus=10*2.00=20, total_delta=30"
}
```

## Key Features

### 1. Predictable Mathematics
- All calculations use integer arithmetic with nearest-integer rounding
- No floating-point precision issues
- Deterministic results for given inputs

### 2. Configurable Thresholds
- Bonus thresholds can be adjusted via configuration
- LOC divisors are hardcoded but could be made configurable
- Main version mod is configurable (default: 1000)

### 3. Comprehensive Testing
- Extensive test suite in `test-workflows/core-tests/`
- Tests cover edge cases, rollovers, and large numbers
- Integration tests verify the complete workflow

### 4. Detailed Reasoning
- All calculations include detailed reasoning
- Helps with debugging and understanding version changes
- Available in all output formats

## Verification

The algorithm has been thoroughly tested and verified:

1. ✅ Base delta formulas implemented correctly
2. ✅ Bonus multiplier system follows specified rules
3. ✅ Mathematical rollover with configurable mod
4. ✅ Semantic analysis provides suggestions based on bonus thresholds
5. ✅ Version calculator applies both LOC and bonus logic
6. ✅ All edge cases and large numbers handled correctly
7. ✅ Multiple output formats provide complete information

The versioning system provides a robust, predictable, and well-documented algorithm for version calculation that balances semantic analysis with mathematical precision.
