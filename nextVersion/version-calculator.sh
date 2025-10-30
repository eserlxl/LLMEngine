#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Version calculator for next-version
# Calculates version bumps based on semantic analysis

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C

# Source common utilities
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# ----- guards ----------------------------------------------------------------
(( BASH_VERSINFO[0] >= 4 )) || { echo "Error: requires Bash ≥ 4" >&2; exit 1; }

show_help() {
    cat << 'EOF'
Version Calculator

Usage:
  $(basename "$0") [options]

Options:
  --current-version <ver>  Current version (e.g., 1.2.3)                 (required)
  --bump-type <type>       One of: major, minor, patch                    (required)
  --loc <n>                Lines of code changed (non-negative integer)   (default: 0)
  --bonus <n>              Bonus points (non-negative integer)            (default: 0)
  --main-mod <n>           MAIN_VERSION_MOD (positive integer)            (default: 1000)
  --machine                Output key=value
  --json                   Output JSON (takes precedence over --machine)
  --quiet, -q              Print only the next version (no labels)
  --strict                 Fail on invalid --current-version (no 0.0.0 fallback)
  --help, -h               Show this help

LOC divisors (for base/bonus scale):
  patch: 250   minor: 500   major: 1000

Examples:
  $(basename "$0") --current-version 1.2.3 --bump-type minor --loc 500
  $(basename "$0") --current-version 1.2.3 --bump-type major --bonus 10 --machine
  $(basename "$0") --current-version 1.2.3 --bump-type patch --loc 100 --json
EOF
}

# ----- utilities -------------------------------------------------------------
# Provide fallbacks for functions not in version-utils.sh
# has_cmd() function is now replaced with require_cmd() from version-utils.sh
to_lower() { printf '%s' "${1,,}"; }
is_semver_xyz() { [[ "$1" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; }

# round_div a/b with nearest-integer rounding
# works for non-negative integers only (we validate inputs as non-negative)
round_div() {
    local a="$1" b="$2"
    # prevent div-by-zero
    (( b == 0 )) && die "internal error: division by zero"
    echo $(( (a + b/2) / b ))
}

# Render n/100 as fixed 2 decimals. n is non-negative integer.
fmt_fixed2_from_int100() {
    local n="$1"
    local whole=$(( n / 100 ))
    local frac=$(( n % 100 ))
    printf '%d.%02d' "$whole" "$frac"
}

# ----- defaults --------------------------------------------------------------
CURRENT_VERSION=""
BUMP_TYPE=""
LOC=0
BONUS=0
MACHINE_OUTPUT=false
JSON_OUTPUT=false
QUIET_OUTPUT=false
STRICT=false
MAIN_VERSION_MOD=1000
BONUS_MULTIPLIER_CAP=""  # Will be loaded from configuration

# ----- parse args ------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --current-version)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--current-version requires a value"
            CURRENT_VERSION="$2"; shift 2;;
        --bump-type)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--bump-type requires a value"
            BUMP_TYPE="$(to_lower "$2")"; shift 2;;
        --loc)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--loc requires a value"
            LOC="$2"; shift 2;;
        --bonus)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--bonus requires a value"
            BONUS="$2"; shift 2;;
        --main-mod)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--main-mod requires a value"
            MAIN_VERSION_MOD="$2"; shift 2;;
        --machine) MACHINE_OUTPUT=true; shift;;
        --json) JSON_OUTPUT=true; shift;;
        --quiet|-q) QUIET_OUTPUT=true; shift;;
        --strict) STRICT=true; shift;;
        --help|-h) show_help; exit 0;;
        *) die "Unknown option: $1";;
    esac
done

# ----- validation ------------------------------------------------------------
[[ -n "$CURRENT_VERSION" ]] || die "--current-version is required"
[[ -n "$BUMP_TYPE"    ]] || die "--bump-type is required"

case "$BUMP_TYPE" in major|minor|patch) ;; *) die "--bump-type must be major, minor, or patch";; esac
is_uint "$LOC"   || die "--loc must be a non-negative integer"
is_uint "$BONUS" || die "--bonus must be a non-negative integer"
# shellcheck disable=SC2015
is_uint "$MAIN_VERSION_MOD" && (( MAIN_VERSION_MOD >= 1 )) || die "--main-mod must be a positive integer"

# ----- load configuration -----------------------------------------------------
# Load bonus multiplier cap from configuration if available
config_file="${SCRIPT_DIR}/../config/versioning.yml"
if [[ -f "$config_file" ]]; then
    cap_value=""
    if cap_value=$(yq -r '.bonus_multiplier_cap // empty' "$config_file" 2>/dev/null); then
        if [[ -n "$cap_value" && "$cap_value" =~ ^[0-9]+\.?[0-9]*$ ]]; then
            BONUS_MULTIPLIER_CAP="$cap_value"
        fi
    fi
fi

# Set default value if not loaded from configuration
if [[ -z "$BONUS_MULTIPLIER_CAP" ]]; then
    BONUS_MULTIPLIER_CAP="5.0"  # Default fallback value
fi

# ----- parse semver (fallback or strict) -------------------------------------
major=0 minor=0 patch=0
if is_semver_xyz "$CURRENT_VERSION"; then
    major="${BASH_REMATCH[1]}"
    minor="${BASH_REMATCH[2]}"
    patch="${BASH_REMATCH[3]}"
else
    if $STRICT; then
        die "--current-version must be in form X.Y.Z (strict mode)"
    fi
    # fallback remains 0.0.0
fi

# ----- output helpers --------------------------------------------------------
emit_json() {
    printf '{\n'
    printf '  "current_version": "%s",\n' "$CURRENT_VERSION"
    printf '  "bump_type": "%s",\n' "$BUMP_TYPE"
    printf '  "next_version": "%s",\n' "$NEXT_VERSION"
    printf '  "loc": %s,\n' "$LOC"
    printf '  "bonus": %s,\n' "$BONUS"
    printf '  "base_delta": %s,\n' "$BASE_DELTA"
    printf '  "bonus_multiplier": "%s",\n' "$BONUS_MULTIPLIER_STR"
    printf '  "total_bonus": %s,\n' "$TOTAL_BONUS"
    printf '  "total_delta": %s,\n' "$TOTAL_DELTA"
    printf '  "main_version_mod": %s,\n' "$MAIN_VERSION_MOD"
    printf '  "loc_divisor": %s,\n' "$LOC_DIVISOR"
    printf '  "reason": "%s"\n' "$REASON"
    printf '}\n'
}

emit_machine() {
    printf 'CURRENT_VERSION=%s\n'  "$CURRENT_VERSION"
    printf 'BUMP_TYPE=%s\n'        "$BUMP_TYPE"
    printf 'NEXT_VERSION=%s\n'     "$NEXT_VERSION"
    printf 'LOC=%s\n'              "$LOC"
    printf 'BONUS=%s\n'            "$BONUS"
    printf 'BASE_DELTA=%s\n'       "$BASE_DELTA"
    printf 'BONUS_MULTIPLIER=%s\n' "$BONUS_MULTIPLIER_STR"
    printf 'TOTAL_BONUS=%s\n'      "$TOTAL_BONUS"
    printf 'TOTAL_DELTA=%s\n'      "$TOTAL_DELTA"
    printf 'MAIN_VERSION_MOD=%s\n' "$MAIN_VERSION_MOD"
    printf 'LOC_DIVISOR=%s\n'      "$LOC_DIVISOR"
    printf 'REASON=%s\n'           "$REASON"
}

emit_human() {
    printf '=== Version Calculation ===\n'
    printf 'Current version: %s\n' "$CURRENT_VERSION"
    printf 'Bump type: %s\n'       "$BUMP_TYPE"
    printf 'Next version: %s\n'    "$NEXT_VERSION"
    printf '\nCalculation Details:\n'
    printf '  Lines of code: %s\n'     "$LOC"
    printf '  Base bonus: %s\n'        "$BONUS"
    printf '  Base delta: %s\n'        "$BASE_DELTA"
    printf '  Bonus multiplier: %s\n'  "$BONUS_MULTIPLIER_STR"
    printf '  Total bonus: %s\n'       "$TOTAL_BONUS"
    printf '  Total delta: %s\n'       "$TOTAL_DELTA"
    printf '  Main version mod: %s\n'  "$MAIN_VERSION_MOD"
    printf '  LOC divisor: %s\n'       "$LOC_DIVISOR"
    printf '\nReason: %s\n' "$REASON"
}

emit_plain() { printf '%s\n' "$NEXT_VERSION"; }

# ----- initial 0.0.0 handling ------------------------------------------------
# If the effective parsed version is 0.0.0 (either explicit or fallback), we
# emit the first version per bump type and keep deltas as zeros.
if (( major == 0 && minor == 0 && patch == 0 )); then
    case "$BUMP_TYPE" in
        major) NEXT_VERSION="1.0.0" ;;
        minor) NEXT_VERSION="0.1.0" ;;
        patch) NEXT_VERSION="0.0.1" ;;
    esac
    
    BASE_DELTA=0
    TOTAL_BONUS=0
    TOTAL_DELTA=0
    LOC_DIVISOR=0
    BONUS_MULTIPLIER_STR="1.00"
    REASON="Initial version from 0.0.0"
    
    $QUIET_OUTPUT && { emit_plain; exit 0; }
    $JSON_OUTPUT && { emit_json; exit 0; }
    $MACHINE_OUTPUT && { emit_machine; exit 0; }
    emit_human; exit 0
fi

# ----- divisors & base delta -------------------------------------------------
# LOC divisor used both for base delta slope and for bonus multiplier (1 + LOC/L)
case "$BUMP_TYPE" in
    patch) LOC_DIVISOR=250 ;;
    minor) LOC_DIVISOR=500 ;;
    major) LOC_DIVISOR=1000 ;;
esac

# Given in original:
#  patch: round(1   * (1 + LOC/250))  == 1 + round(LOC/250)
#  minor: round(5   * (1 + LOC/500))  == 5 + round(LOC/100)
#  major: round(10  * (1 + LOC/1000)) == 10 + round(LOC/100)
calc_base_delta() {
    local bt="$1" loc="$2"
    local base=1 add=0
    case "$bt" in
        patch) base=1;   add=$(round_div "$((loc))" 250) ;;
        minor) base=5;   add=$(round_div "$((loc))" 100) ;;
        major) base=10;  add=$(round_div "$((loc))" 100) ;;
        *) base=1; add=0 ;;
    esac
    local out=$(( base + add ))
    (( out < 1 )) && out=1
    echo "$out"
}

BASE_DELTA="$(calc_base_delta "$BUMP_TYPE" "$LOC")"

# ----- bonus multiplier and total bonus --------------------------------------
# bonus_multiplier = 1 + LOC/loc_divisor (rendered as 2 decimals for output)
# total_bonus = round(BONUS * (1 + LOC/loc_divisor)) = BONUS + round(BONUS*LOC/loc_divisor)
# Keep math integer, only render multiplier as string w/2 decimals.
bonus_scale_100=$(( (100 * (LOC_DIVISOR + LOC) + LOC_DIVISOR/2) / LOC_DIVISOR ))   # == round(100*(1+LOC/L))

# Apply bonus multiplier cap to prevent excessive version increases
raw_multiplier=""
raw_multiplier=$(awk "BEGIN {printf \"%.2f\", $bonus_scale_100 / 100.0}")
capped_multiplier=""
capped_multiplier=$(awk "BEGIN {mult = $raw_multiplier; cap = $BONUS_MULTIPLIER_CAP; printf \"%.2f\", (mult > cap) ? cap : mult}")
capped_scale_100=""
capped_scale_100=$(awk "BEGIN {printf \"%.0f\", $capped_multiplier * 100}")

BONUS_MULTIPLIER_STR="$(fmt_fixed2_from_int100 "$capped_scale_100")"

# Calculate total bonus using the capped multiplier
capped_bonus_extra=""
capped_bonus_extra=$(awk "BEGIN {printf \"%.0f\", $BONUS * $capped_multiplier - $BONUS}")
TOTAL_BONUS=$(( BONUS + capped_bonus_extra ))

# ----- total delta -----------------------------------------------------------
TOTAL_DELTA=$(( BASE_DELTA + TOTAL_BONUS ))
(( TOTAL_DELTA < 1 )) && TOTAL_DELTA=1

# ----- rollover math ---------------------------------------------------------
# z_new = (patch + TOTAL_DELTA)
# delta_y = floor(z_new / MAIN_VERSION_MOD)
# y_new = minor + delta_y
# delta_x = floor(y_new / MAIN_VERSION_MOD)
# final = (major + delta_x).(y_new % mod).(z_new % mod)
z_new=$(( patch + TOTAL_DELTA ))
delta_y=$(( z_new / MAIN_VERSION_MOD ))
final_z=$(( z_new % MAIN_VERSION_MOD ))

y_new=$(( minor + delta_y ))
delta_x=$(( y_new / MAIN_VERSION_MOD ))
final_y=$(( y_new % MAIN_VERSION_MOD ))

final_x=$(( major + delta_x ))
NEXT_VERSION="${final_x}.${final_y}.${final_z}"

REASON=$(
    printf 'LOC=%s, %s update, base_delta=%s, bonus=%s*%s=%s, total_delta=%s' \
        "$LOC" "${BUMP_TYPE^^}" "$BASE_DELTA" "$BONUS" "$BONUS_MULTIPLIER_STR" "$TOTAL_BONUS" "$TOTAL_DELTA"
)

# ----- emit ------------------------------------------------------------------
$QUIET_OUTPUT && { emit_plain; exit 0; }
$JSON_OUTPUT && { emit_json; exit 0; }
$MACHINE_OUTPUT && { emit_machine; exit 0; }
emit_human 