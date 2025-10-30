#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Version validator for next-version
# Handles version format validation and related checks

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C
# Prevent pagers/locks in CI for speed and determinism
export GIT_PAGER=cat PAGER=cat GIT_OPTIONAL_LOCKS=0

# Source utilities
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/version-utils.sh"

# Initialize colors only for standalone usage; library users can call directly.
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    init_colors "${NO_COLOR:-false}"
fi

# --- SemVer parsing and comparison helpers -----------------------------------
# Strip build metadata: 1.2.3-rc.1+abc -> 1.2.3-rc.1
strip_build_meta() {
    local v="$1"
    printf '%s' "${v%%+*}"
}

# Check if identifier is numeric (no leading zeros unless value is 0)
_is_numeric_ident() {
    [[ "$1" =~ ^(0|[1-9][0-9]*)$ ]]
}

# Compare two pre-release identifier lists (SemVer §11).
# echo -1|0|1
_compare_prerelease() {
    local pr1="$1" pr2="$2"
    # This function is only called when BOTH are non-empty pre-releases
    local IFS='.'
    read -r -a A <<< "$pr1"
    read -r -a B <<< "$pr2"

    local i maxlen=$(( ${#A[@]} > ${#B[@]} ? ${#A[@]} : ${#B[@]} ))
    for (( i=0; i<maxlen; i++ )); do
        local a="${A[i]:-__MISSING__}"
        local b="${B[i]:-__MISSING__}"
        if [[ "$a" == "__MISSING__" && "$b" == "__MISSING__" ]]; then
            printf '0'; return 0
        elif [[ "$a" == "__MISSING__" ]]; then
            # fewer identifiers => lower precedence
            printf '%s' "-1"; return 0
        elif [[ "$b" == "__MISSING__" ]]; then
            printf '%s' "1"; return 0
        fi

        local a_num=0 b_num=0
        _is_numeric_ident "$a" && a_num=1
        _is_numeric_ident "$b" && b_num=1

        if (( a_num == 1 && b_num == 1 )); then
            # numeric vs numeric: numeric compare
            if (( 10#$a < 10#$b )); then printf '%s' "-1"; return 0; fi
            if (( 10#$a > 10#$b )); then printf '%s' "1";  return 0; fi
        elif (( a_num == 1 && b_num == 0 )); then
            # numeric < non-numeric
            printf '%s' "-1"; return 0
        elif (( a_num == 0 && b_num == 1 )); then
            printf '%s' "1"; return 0
        else
            # alpha vs alpha: ASCII lexical
            if [[ "$a" < "$b" ]]; then printf '%s' "-1"; return 0; fi
            if [[ "$a" > "$b" ]]; then printf '%s' "1";  return 0; fi
        fi
        # else equal at this field; continue
    done
    printf '0'
}

# --- Version validation -------------------------------------------------------
validate_version_format() {
    local version="$1"
    local allow_prerelease="$2"

    if [[ "$allow_prerelease" == "true" ]]; then
        if ! _is_semver_with_prerelease "$version"; then
            printf '%s\n' "${YELLOW}Expected: MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD] (e.g., 1.0.0, 1.0.0-rc.1)${RESET}" >&2
            printf '%s\n' "${YELLOW}Note: Leading zeros are not allowed${RESET}" >&2
            _die "Invalid version format: $version"
        fi
    else
        if ! _is_semver_core "$version"; then
            printf '%s\n' "${YELLOW}Expected: MAJOR.MINOR.PATCH (e.g., 1.0.0)${RESET}" >&2
            printf '%s\n' "${YELLOW}Note: Leading zeros are not allowed${RESET}" >&2
            printf '%s\n' "${YELLOW}Note: Pre-releases require --allow-prerelease with --set${RESET}" >&2
            _die "Invalid version format: $version"
        fi
    fi
}

validate_version_file() {
    local version_file="$1"
    local project_root="$2"
    
    if [[ ! -f "$version_file" ]]; then
        return 1
    fi
    
    validate_version_file_path "$version_file" "$project_root"
    
    local current_version
    current_version=$(read_version_file "$version_file")
    if [[ ! "$current_version" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
        printf '%s\n' "${YELLOW}Expected: MAJOR.MINOR.PATCH${RESET}" >&2
        _die "Invalid version format in VERSION: $current_version"
    fi
    
    printf '%s' "$current_version"
}

# --- Version comparison -------------------------------------------------------
# Public: compare two versions using full SemVer. echo -1|0|1
compare_versions() {
    local v1="$1" v2="$2"
    
    # Strip any build metadata first
    local clean_v1="${v1%%+*}" clean_v2="${v2%%+*}"
    
    # Split into main version and prerelease
    local m1="${clean_v1%%-*}" pr1="${clean_v1#*-}"
    local m2="${clean_v2%%-*}" pr2="${clean_v2#*-}"
    
    # If no prerelease, set to empty
    [[ "$pr1" == "$m1" ]] && pr1=""
    [[ "$pr2" == "$m2" ]] && pr2=""
    
    # Parse main versions
    local v1_major v1_minor v1_patch v2_major v2_minor v2_patch
    
    IFS='.' read -r v1_major v1_minor v1_patch <<< "$m1"
    IFS='.' read -r v2_major v2_minor v2_patch <<< "$m2"
    
    # Compare major (using proper numeric comparison)
    if (( 10#$v1_major < 10#$v2_major )); then
        printf '%s' "-1"
    elif (( 10#$v1_major > 10#$v2_major )); then
        printf '%s' "1"
    else
        # Compare minor
        if (( 10#$v1_minor < 10#$v2_minor )); then
            printf '%s' "-1"
        elif (( 10#$v1_minor > 10#$v2_minor )); then
            printf '%s' "1"
        else
            # Compare patch
            if (( 10#$v1_patch < 10#$v2_patch )); then
                printf '%s' "-1"
            elif (( 10#$v1_patch > 10#$v2_patch )); then
                printf '%s' "1"
            else
                # Versions are equal, compare prereleases
                if [[ -z "$pr1" && -z "$pr2" ]]; then
                    printf '%s' "0"
                elif [[ -z "$pr1" && -n "$pr2" ]]; then
                    printf '%s' "1"
                elif [[ -n "$pr1" && -z "$pr2" ]]; then
                    printf '%s' "-1"
                else
                    # Both have prereleases, use proper SemVer precedence
                    _compare_prerelease "$pr1" "$pr2"
                fi
            fi
        fi
    fi
}

is_version_greater() {
    local new_version="$1"
    local last_version="$2"
    
    [[ "$(compare_versions "$new_version" "$last_version")" == "1" ]]
}

# --- Version order validation -------------------------------------------------
# Only check monotonicity for *stable* target (skip if new is prerelease)
check_version_order() {
    local new_version="$1"
    local tag_prefix="$2"
    local allow_nonmonotonic="$3"
    
    if [[ "$new_version" == *-* ]]; then
        return 0
    fi
    
    local last_tag
    last_tag="$(last_tag_for_prefix "$tag_prefix" || true)"

    if [[ -n "$last_tag" ]]; then
        local last_version="${last_tag:${#tag_prefix}}"
        # If last tag isn't strict semver, we don't enforce
        if ! _is_semver_core "$last_version"; then
            return 0
        fi

        if ! is_version_greater "$new_version" "$last_version"; then
            _warn "New version $new_version is not greater than last tag $last_tag"
            if [[ -n "${GITHUB_ACTIONS:-}" && "$allow_nonmonotonic" != "true" ]]; then
                printf '%s\n' "${YELLOW}Use --allow-nonmonotonic-tag to override in CI.${RESET}" >&2
                _die "NEW_VERSION ($new_version) must be greater than last tag ($last_tag)"
            fi
        fi
    fi
}

# --- Version parsing ---------------------------------------------------------
parse_version_components() {
    local version="$1"
    local major minor patch
    
    # Strip build metadata and prerelease
    local main_version="${version%%+*}"
    main_version="${main_version%%-*}"
    
    IFS='.' read -r major minor patch <<< "$main_version"
    printf '%s\n%s\n%s' "$major" "$minor" "$patch"
}

# --- Pre-release validation --------------------------------------------------
is_prerelease() {
    local version="$1"
    [[ "$(strip_build_meta "$version")" == *-* ]]
}

validate_prerelease_format() {
    local version="$1"
    if is_prerelease "$version"; then
        if ! _is_semver_with_prerelease "$version"; then
            printf '%s\n' "${YELLOW}Expected: MAJOR.MINOR.PATCH-PRERELEASE (e.g., 1.0.0-rc.1)${RESET}" >&2
            _die "Invalid pre-release format: $version"
        fi
    fi
}

# --- Main validation function -------------------------------------------------
validate_version_input() {
    local version="$1"
    local allow_prerelease="$2"
    local tag_prefix="$3"
    local allow_nonmonotonic="$4"
    
    # Validate format
    validate_version_format "$version" "$allow_prerelease"
    
    # Check version order if not a pre-release
    if ! is_prerelease "$version"; then
        check_version_order "$version" "$tag_prefix" "$allow_nonmonotonic"
    fi
}

# --- CLI usage ---------------------------------------------------------------
usage() {
    cat <<'EOF'
Usage: version-validator <command> [args...]

Commands:
  validate <version> [allow_prerelease]   Validate version format
  compare <version1> <version2>           Compare two versions (-1|0|1) with full SemVer precedence
  parse <version>                          Print MAJOR NEWLINE MINOR NEWLINE PATCH
  is-prerelease <version>                  Print true/false; exits 0 for true, 1 for false

Extras (optional):
  validate-file <path> <project_root>      Validate VERSION file (strict MAJOR.MINOR.PATCH)
  order-check <version> <tag_prefix> [allow_nonmonotonic=true|false]
                                           Enforce monotonic increase vs last tag (stable only)

Notes:
- Pre-release precedence is implemented per SemVer 2.0.0 (numeric < alphanumeric, field-wise compare).
- Build metadata (+meta) is ignored in precedence checks, as per spec.
EOF
}

# --- Standalone usage --------------------------------------------------------
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    cmd="${1:-}"; shift || true
    case "$cmd" in
        "validate")
            if [[ $# -lt 1 ]]; then
                _die "Usage: $0 validate <version> [allow_prerelease]"
            fi
            validate_version_format "$1" "${2:-false}"
            _ok "Version format is valid"
            ;;
        "compare")
            if [[ $# -lt 2 ]]; then
                _die "Usage: $0 compare <version1> <version2>"
            fi
            result=$(compare_versions "$1" "$2")
            printf '%s\n' "$result"
            ;;
        "parse")
            if [[ $# -lt 1 ]]; then
                _die "Usage: $0 parse <version>"
            fi
            parse_version_components "$1"
            ;;
        "is-prerelease")
            if [[ $# -lt 1 ]]; then
                _die "Usage: $0 is-prerelease <version>"
            fi
            if is_prerelease "$1"; then
                printf '%s\n' "true"
                exit 0
            else
                printf '%s\n' "false"
                exit 1
            fi
            ;;
        "validate-file")
            if [[ $# -lt 2 ]]; then
                _die "Usage: $0 validate-file <path> <project_root>"
            fi
            validate_version_file "$1" "$2" >/dev/null
            _ok "VERSION file is valid"
            ;;
        "order-check")
            if [[ $# -lt 2 ]]; then
                _die "Usage: $0 order-check <version> <tag_prefix> [allow_nonmonotonic=true|false]"
            fi
            check_version_order "$1" "$2" "${3:-false}"
            ;;
        -h|--help|help|"")
            usage
            ;;
        *)
            _die "Unknown command: $cmd" "$(usage)"
            ;;
    esac
fi 