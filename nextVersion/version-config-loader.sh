#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Version config loader for next-version
# Loads versioning configuration from YAML files

set -Eeuo pipefail
shopt -s lastpipe
IFS=$'\n\t'
export LC_ALL=C
umask 022

# Source common utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# ---------- program id / guards ----------
readonly PROG="${0##*/}"
(( BASH_VERSINFO[0] >= 4 )) || { printf '%s\n' "Error: $PROG requires Bash ≥ 4" >&2; exit 2; }

# ---------- paths ----------
DEFAULT_CONFIG="$SCRIPT_DIR/../config/versioning.yml"
CONFIG_FILE="$DEFAULT_CONFIG"

# ---------- help ----------
show_help() {
    cat << EOF
Version Configuration Loader

Usage: $PROG [options]

Options:
  --config-file <path>     Path to configuration file (default: $DEFAULT_CONFIG; use '-' for stdin)
  --validate-only          Only validate configuration without output
  --machine                Output machine-readable key=value format
  --json                   Output machine-readable JSON
  --help, -h               Show this help

Environment Variables (fallback):
  VERSION_PATCH_LIMIT      Patch version limit before rollover (default: 100)
  VERSION_MINOR_LIMIT      Minor version limit before rollover (default: 100)
  VERSION_PATCH_DELTA      Patch delta formula (default: 1*(1+LOC/250))
  VERSION_MINOR_DELTA      Minor delta formula (default: 5*(1+LOC/500))
  VERSION_MAJOR_DELTA      Major delta formula (default: 10*(1+LOC/1000))
  VERSION_BREAKING_CLI_BONUS Breaking CLI bonus (default: 2)
  VERSION_API_BREAKING_BONUS API breaking bonus (default: 3)
  VERSION_REMOVED_OPTION_BONUS Removed option bonus (default: 1)
  VERSION_CLI_CHANGES_BONUS CLI changes bonus (default: 2)
  VERSION_MANUAL_CLI_BONUS Manual CLI bonus (default: 1)
  VERSION_NEW_SOURCE_BONUS New source file bonus (default: 1)
  VERSION_NEW_TEST_BONUS   New test file bonus (default: 1)
  VERSION_NEW_DOC_BONUS    New doc file bonus (default: 1)
  VERSION_ADDED_OPTION_BONUS Added option bonus (default: 1)
  VERSION_SECURITY_BONUS   Security keyword bonus (default: 2)

Examples:
  $PROG --validate-only
  $PROG --machine
  $PROG --json
EOF
}

# ---------- logging / utils ----------
# warn() and die() functions are now sourced from version-utils.sh

trap 'warn "line $LINENO: $BASH_COMMAND"' ERR

# test command presence
has() { command -v "$1" >/dev/null 2>&1; }

# Numeric validators (support integers/floats)
is_number()        { [[ "${1:-}" =~ ^-?[0-9]+([.][0-9]+)?$ ]]; }
is_positive()      { is_number "$1" && awk -v v="$1" 'BEGIN{exit !(v>0)}'; }
is_nonneg_number() { is_number "$1" && awk -v v="$1" 'BEGIN{exit !(v>=0)}'; }

# JSON helpers
# json_escape() function is now sourced from version-utils.sh
num_or_null() { [[ -n "${1:-}" ]] && printf '%s' "$1" || printf 'null'; }
str_or_null() { [[ -n "${1:-}" ]] && { printf '"'; json_escape "$1"; printf '"'; } || printf 'null'; }

# ---------- argument parsing ----------
VALIDATE_ONLY=false
MACHINE_OUTPUT=false
JSON_OUTPUT=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --config-file)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--config-file requires a value"
            CONFIG_FILE="$2"; shift 2;;
        --validate-only) VALIDATE_ONLY=true; shift;;
        --machine) MACHINE_OUTPUT=true; shift;;
        --json) JSON_OUTPUT=true; shift;;
        --help|-h) show_help; exit 0;;
        *) die "Unknown option: $1";;
    esac
done

if $MACHINE_OUTPUT && $JSON_OUTPUT; then
    die "Use only one of --json or --machine"
fi

# ---------- yq helpers ----------
yq_present=false
yaml_present=false
yq_v4=false

if has yq; then
    yq_present=true
    # verify v3+ (we rely on 'yq' command)
    if yq --version 2>/dev/null | grep -Eq '[34]\.[0-9]'; then
        yq_v4=true
    fi
    if [[ "$CONFIG_FILE" == "-" ]]; then
        # If reading from stdin, we need to have data; skip emptiness check (assume caller knows).
        yaml_present=true
    else
        [[ -f "$CONFIG_FILE" ]] && yaml_present=true
    fi
fi

# Read from file or stdin depending on CONFIG_FILE
_yq_base() {
    # yq v3 syntax compatible
    if [[ "$CONFIG_FILE" == "-" ]]; then
        yq "$1" -
    else
        yq "$1" "$CONFIG_FILE"
    fi
}

# Returns empty string for null/missing; raw scalar only
yq_get_raw() {
    _yq_base "$1 // \"\"" 2>/dev/null | sed 's/^null$//'
}

# Strict numeric fetcher (returns empty if not numeric)
yq_get_num() {
    local v
    v="$(yq_get_raw "$1")"
    if [[ -n "$v" ]] && is_number "$v"; then
        printf '%s' "$v"
    else
        printf ''
    fi
}

# Pass-through string (returns empty if missing)
yq_get_str() {
    local v
    v="$(yq_get_raw "$1")"
    printf '%s' "$v"
}

# ---------- load config from YAML (preferred) ----------
# Values loaded here are *source-of-truth* when present.
# The documented "environment variables" are used later only as fallback defaults.
LOC_CAP=""
ROLLOVER=""
MAJOR_BONUS_THRESHOLD=""
MINOR_BONUS_THRESHOLD=""
PATCH_BONUS_THRESHOLD=""
LOC_DIVISOR_MAJOR=""
LOC_DIVISOR_MINOR=""
LOC_DIVISOR_PATCH=""
EARLY_EXIT_BONUS_THRESHOLD=""
EARLY_EXIT_CHANGE_TYPE=""
MEMORY_REDUCTION_THRESHOLD=""
BUILD_TIME_THRESHOLD=""
PERF_50_THRESHOLD=""
BONUS_API_BREAKING=""
BONUS_CLI_BREAKING=""
BONUS_SECURITY_VULN=""
BONUS_CVE=""
BONUS_MEMORY_SAFETY=""
BONUS_CRASH_FIX=""
BONUS_NEW_CLI_COMMAND=""
BONUS_NEW_CONFIG_OPTION=""
BONUS_NEW_SOURCE_FILE=""
BONUS_NEW_TEST_FILE=""
BONUS_NEW_DOC_FILE=""
MULTIPLIER_ZERO_DAY=""
MULTIPLIER_PRODUCTION_OUTAGE=""
MULTIPLIER_COMPLIANCE=""
BASE_DELTA_PATCH=""
BASE_DELTA_MINOR=""
BASE_DELTA_MAJOR=""

load_config() {
    if ! $yq_present; then
        warn "yq not found; using env/hard defaults."
        return 1
    fi
    if ! $yq_v4; then
        warn "yq v3+ is required; detected a different version. Using env/hard defaults."
        return 1
    fi
    if ! $yaml_present; then
        warn "Config file '${CONFIG_FILE}' not found; using env/hard defaults."
        return 1
    fi

    # base deltas
    BASE_DELTA_PATCH="$(yq_get_num '.base_deltas.patch')"
    BASE_DELTA_MINOR="$(yq_get_num '.base_deltas.minor')"
    BASE_DELTA_MAJOR="$(yq_get_num '.base_deltas.major')"

    # limits / thresholds
    LOC_CAP="$(yq_get_num '.limits.loc_cap')"
    ROLLOVER="$(yq_get_num '.limits.rollover')"

    MAJOR_BONUS_THRESHOLD="$(yq_get_num '.thresholds.major_bonus')"
    MINOR_BONUS_THRESHOLD="$(yq_get_num '.thresholds.minor_bonus')"
    PATCH_BONUS_THRESHOLD="$(yq_get_num '.thresholds.patch_bonus')"

    # loc divisors (must be > 0 if set)
    LOC_DIVISOR_MAJOR="$(yq_get_num '.loc_divisors.major')"
    LOC_DIVISOR_MINOR="$(yq_get_num '.loc_divisors.minor')"
    LOC_DIVISOR_PATCH="$(yq_get_num '.loc_divisors.patch')"

    # Validate LOC divisors (guard against zero)
    for name in major minor patch; do
        local var="LOC_DIVISOR_${name^^}"
        local val="${!var:-}"
        if [[ -n "$val" ]] && ! is_positive "$val"; then
            die "Invalid LOC divisor for $name: $val (must be > 0)"
        fi
    done

    # early exit
    EARLY_EXIT_BONUS_THRESHOLD="$(yq_get_num '.patterns.early_exit.bonus_threshold')"
    EARLY_EXIT_CHANGE_TYPE="$(yq_get_str '.patterns.early_exit.change_type')"

    # performance thresholds
    MEMORY_REDUCTION_THRESHOLD="$(yq_get_num '.patterns.performance.memory_reduction_threshold')"
    BUILD_TIME_THRESHOLD="$(yq_get_num '.patterns.performance.build_time_threshold')"
    PERF_50_THRESHOLD="$(yq_get_num '.patterns.performance.perf_50_threshold')"

    # bonuses
    BONUS_API_BREAKING="$(yq_get_num '.bonuses.breaking_changes.api_breaking')"
    BONUS_CLI_BREAKING="$(yq_get_num '.bonuses.breaking_changes.cli_breaking')"
    BONUS_REMOVED_FEATURES="$(yq_get_num '.bonuses.breaking_changes.removed_features')"
    BONUS_SECURITY_VULN="$(yq_get_num '.bonuses.security_stability.security_vuln')"
    BONUS_CVE="$(yq_get_num '.bonuses.security_stability.cve')"
    BONUS_MEMORY_SAFETY="$(yq_get_num '.bonuses.security_stability.memory_safety')"
    BONUS_CRASH_FIX="$(yq_get_num '.bonuses.security_stability.crash_fix')"
    BONUS_NEW_CLI_COMMAND="$(yq_get_num '.bonuses.features.new_cli_command')"
    BONUS_NEW_CONFIG_OPTION="$(yq_get_num '.bonuses.features.new_config_option')"
    BONUS_NEW_SOURCE_FILE="$(yq_get_num '.bonuses.code_quality.new_source_file')"
    BONUS_NEW_TEST_FILE="$(yq_get_num '.bonuses.code_quality.new_test_suite')"
    BONUS_NEW_DOC_FILE="$(yq_get_num '.bonuses.code_quality.doc_overhaul')"

    # multipliers (critical.*). If present, validate numeric and export canonical names.
    MULTIPLIER_ZERO_DAY="$(yq_get_num '.multipliers.critical.zero_day')"
    MULTIPLIER_PRODUCTION_OUTAGE="$(yq_get_num '.multipliers.critical.production_outage')"
    MULTIPLIER_COMPLIANCE="$(yq_get_num '.multipliers.critical.compliance')"

    return 0
}

# Load configuration from YAML file
load_config || true

# ---------- defaults / fallbacks ----------
# PURELY MATHEMATICAL VERSIONING SYSTEM
# All version bump decisions are based on bonus point calculations
# No minimum thresholds or extra rules - pure math logic only

# ROLLOVER used for both patch/minor limits if not set
VERSION_PATCH_LIMIT="${VERSION_PATCH_LIMIT:-${ROLLOVER:-100}}"
VERSION_MINOR_LIMIT="${VERSION_MINOR_LIMIT:-${ROLLOVER:-100}}"

# LOC delta formulas:
# If YAML has divisors/deltas, prefer those; otherwise fallback to documented env/defaults.
if [[ -n "${LOC_DIVISOR_PATCH:-}" ]]; then
    VERSION_PATCH_DELTA="${BASE_DELTA_PATCH:-1}*(1+LOC/${LOC_DIVISOR_PATCH})"
else
    VERSION_PATCH_DELTA="${VERSION_PATCH_DELTA:-1*(1+LOC/250)}"
fi

if [[ -n "${LOC_DIVISOR_MINOR:-}" ]]; then
    VERSION_MINOR_DELTA="${BASE_DELTA_MINOR:-5}*(1+LOC/${LOC_DIVISOR_MINOR})"
else
    VERSION_MINOR_DELTA="${VERSION_MINOR_DELTA:-5*(1+LOC/500)}"
fi

if [[ -n "${LOC_DIVISOR_MAJOR:-}" ]]; then
    VERSION_MAJOR_DELTA="${BASE_DELTA_MAJOR:-10}*(1+LOC/${LOC_DIVISOR_MAJOR})"
else
    VERSION_MAJOR_DELTA="${VERSION_MAJOR_DELTA:-10*(1+LOC/1000)}"
fi

# Bonus values (YAML preferred → fallback to env → hard default)
VERSION_BREAKING_CLI_BONUS="${BONUS_CLI_BREAKING:-${VERSION_BREAKING_CLI_BONUS:-2}}"
VERSION_API_BREAKING_BONUS="${BONUS_API_BREAKING:-${VERSION_API_BREAKING_BONUS:-3}}"
VERSION_REMOVED_OPTION_BONUS="${BONUS_REMOVED_FEATURES:-${VERSION_REMOVED_OPTION_BONUS:-1}}"
VERSION_CLI_CHANGES_BONUS="${VERSION_CLI_CHANGES_BONUS:-2}"
VERSION_MANUAL_CLI_BONUS="${VERSION_MANUAL_CLI_BONUS:-1}"
VERSION_NEW_SOURCE_BONUS="${BONUS_NEW_SOURCE_FILE:-${VERSION_NEW_SOURCE_BONUS:-1}}"
VERSION_NEW_TEST_BONUS="${BONUS_NEW_TEST_FILE:-${VERSION_NEW_TEST_BONUS:-1}}"
VERSION_NEW_DOC_BONUS="${BONUS_NEW_DOC_FILE:-${VERSION_NEW_DOC_BONUS:-1}}"
VERSION_ADDED_OPTION_BONUS="${BONUS_NEW_CONFIG_OPTION:-${VERSION_ADDED_OPTION_BONUS:-1}}"
VERSION_SECURITY_BONUS="${BONUS_SECURITY_VULN:-${VERSION_SECURITY_BONUS:-2}}"

# Thresholds with safe defaults if YAML missing
MAJOR_BONUS_THRESHOLD="${MAJOR_BONUS_THRESHOLD:-8}"
MINOR_BONUS_THRESHOLD="${MINOR_BONUS_THRESHOLD:-4}"
PATCH_BONUS_THRESHOLD="${PATCH_BONUS_THRESHOLD:-0}"

# Early exit configuration
EARLY_EXIT_BONUS_THRESHOLD="${EARLY_EXIT_BONUS_THRESHOLD:-8}"
EARLY_EXIT_CHANGE_TYPE="${EARLY_EXIT_CHANGE_TYPE:-major}"

# Export only intended shared variables
export LOC_CAP MAJOR_BONUS_THRESHOLD MINOR_BONUS_THRESHOLD PATCH_BONUS_THRESHOLD
export MEMORY_REDUCTION_THRESHOLD BUILD_TIME_THRESHOLD PERF_50_THRESHOLD
export MULTIPLIER_ZERO_DAY MULTIPLIER_PRODUCTION_OUTAGE MULTIPLIER_COMPLIANCE
export BONUS_CVE BONUS_MEMORY_SAFETY BONUS_CRASH_FIX BONUS_NEW_CLI_COMMAND

# ---------- output ----------
print_machine_kv() {
    printf 'CONFIG_FILE=%s\n' "$CONFIG_FILE"
    printf 'VERSION_PATCH_LIMIT=%s\n' "$VERSION_PATCH_LIMIT"
    printf 'VERSION_MINOR_LIMIT=%s\n' "$VERSION_MINOR_LIMIT"
    printf 'MAJOR_BONUS_THRESHOLD=%s\n' "$MAJOR_BONUS_THRESHOLD"
    printf 'MINOR_BONUS_THRESHOLD=%s\n' "$MINOR_BONUS_THRESHOLD"
    printf 'PATCH_BONUS_THRESHOLD=%s\n' "$PATCH_BONUS_THRESHOLD"
    printf 'VERSION_BREAKING_CLI_BONUS=%s\n' "$VERSION_BREAKING_CLI_BONUS"
    printf 'VERSION_API_BREAKING_BONUS=%s\n' "$VERSION_API_BREAKING_BONUS"
    printf 'VERSION_REMOVED_OPTION_BONUS=%s\n' "$VERSION_REMOVED_OPTION_BONUS"
    printf 'VERSION_CLI_CHANGES_BONUS=%s\n' "$VERSION_CLI_CHANGES_BONUS"
    printf 'VERSION_MANUAL_CLI_BONUS=%s\n' "$VERSION_MANUAL_CLI_BONUS"
    printf 'VERSION_NEW_SOURCE_BONUS=%s\n' "$VERSION_NEW_SOURCE_BONUS"
    printf 'VERSION_NEW_TEST_BONUS=%s\n' "$VERSION_NEW_TEST_BONUS"
    printf 'VERSION_NEW_DOC_BONUS=%s\n' "$VERSION_NEW_DOC_BONUS"
    printf 'VERSION_ADDED_OPTION_BONUS=%s\n' "$VERSION_ADDED_OPTION_BONUS"
    printf 'VERSION_SECURITY_BONUS=%s\n' "$VERSION_SECURITY_BONUS"
    printf 'EARLY_EXIT_BONUS_THRESHOLD=%s\n' "$EARLY_EXIT_BONUS_THRESHOLD"
    printf 'EARLY_EXIT_CHANGE_TYPE=%s\n' "$EARLY_EXIT_CHANGE_TYPE"
    printf 'VERSION_PATCH_DELTA=%s\n' "$VERSION_PATCH_DELTA"
    printf 'VERSION_MINOR_DELTA=%s\n' "$VERSION_MINOR_DELTA"
    printf 'VERSION_MAJOR_DELTA=%s\n' "$VERSION_MAJOR_DELTA"
}

if $VALIDATE_ONLY; then
    printf 'Configuration validation completed successfully\n'
    exit 0
elif $JSON_OUTPUT; then
    # Only "config_file" and "change_type" may need escaping; deltas are strings by design
    printf '{\n'
    printf '  "config_file": %s,\n'  "$(str_or_null "$CONFIG_FILE")"
    printf '  "loc_delta": {\n'
    printf '    "patch_limit": %s,\n' "$(num_or_null "$VERSION_PATCH_LIMIT")"
    printf '    "minor_limit": %s,\n' "$(num_or_null "$VERSION_MINOR_LIMIT")"
    printf '    "patch_delta": %s,\n'  "$(str_or_null "$VERSION_PATCH_DELTA")"
    printf '    "minor_delta": %s,\n'  "$(str_or_null "$VERSION_MINOR_DELTA")"
    printf '    "major_delta": %s\n'   "$(str_or_null "$VERSION_MAJOR_DELTA")"
    printf '  },\n'
    printf '  "thresholds": {\n'
    printf '    "major_bonus": %s,\n' "$(num_or_null "$MAJOR_BONUS_THRESHOLD")"
    printf '    "minor_bonus": %s,\n' "$(num_or_null "$MINOR_BONUS_THRESHOLD")"
    printf '    "patch_bonus": %s\n'  "$(num_or_null "$PATCH_BONUS_THRESHOLD")"
    printf '  },\n'
    printf '  "bonuses": {\n'
    printf '    "breaking_cli": %s,\n' "$(num_or_null "$VERSION_BREAKING_CLI_BONUS")"
    printf '    "api_breaking": %s,\n' "$(num_or_null "$VERSION_API_BREAKING_BONUS")"
    printf '    "removed_option": %s,\n' "$(num_or_null "$VERSION_REMOVED_OPTION_BONUS")"
    printf '    "cli_changes": %s,\n'   "$(num_or_null "$VERSION_CLI_CHANGES_BONUS")"
    printf '    "manual_cli": %s,\n'    "$(num_or_null "$VERSION_MANUAL_CLI_BONUS")"
    printf '    "new_source": %s,\n'    "$(num_or_null "$VERSION_NEW_SOURCE_BONUS")"
    printf '    "new_test": %s,\n'      "$(num_or_null "$VERSION_NEW_TEST_BONUS")"
    printf '    "new_doc": %s,\n'       "$(num_or_null "$VERSION_NEW_DOC_BONUS")"
    printf '    "added_option": %s,\n'  "$(num_or_null "$VERSION_ADDED_OPTION_BONUS")"
    printf '    "security": %s\n'       "$(num_or_null "$VERSION_SECURITY_BONUS")"
    printf '  },\n'
    printf '  "early_exit": {\n'
    printf '    "bonus_threshold": %s,\n' "$(num_or_null "$EARLY_EXIT_BONUS_THRESHOLD")"
    printf '    "change_type": %s\n'      "$(str_or_null "$EARLY_EXIT_CHANGE_TYPE")"
    printf '  }\n'
    printf '}\n'
elif $MACHINE_OUTPUT; then
    print_machine_kv
else
    printf '=== Version Configuration ===\n'
    printf 'Config file: %s\n' "$CONFIG_FILE"
    printf '\nLOC Delta System:\n'
    printf '  Patch limit: %s\n' "$VERSION_PATCH_LIMIT"
    printf '  Minor limit: %s\n' "$VERSION_MINOR_LIMIT"
    printf '  Patch delta: %s\n' "$VERSION_PATCH_DELTA"
    printf '  Minor delta: %s\n' "$VERSION_MINOR_DELTA"
    printf '  Major delta: %s\n' "$VERSION_MAJOR_DELTA"
    printf '\nThresholds:\n'
    printf '  Major bonus: %s\n' "$MAJOR_BONUS_THRESHOLD"
    printf '  Minor bonus: %s\n' "$MINOR_BONUS_THRESHOLD"
    printf '  Patch bonus: %s\n' "$PATCH_BONUS_THRESHOLD"
    printf '\nBonus Values:\n'
    printf '  Breaking CLI: %s\n' "$VERSION_BREAKING_CLI_BONUS"
    printf '  API breaking: %s\n' "$VERSION_API_BREAKING_BONUS"
    printf '  Security: %s\n'     "$VERSION_SECURITY_BONUS"
    printf '  Early exit: threshold=%s, change_type=%s\n' "$EARLY_EXIT_BONUS_THRESHOLD" "$EARLY_EXIT_CHANGE_TYPE"
fi 