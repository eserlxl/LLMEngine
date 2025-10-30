#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Version calculator with LOC delta for next-version
# Calculates version bumps based on lines of code changes

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C

# Source common utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# ---------- traps & ids -------------------------------------------------------
readonly PROG="${0##*/}"
trap 'echo "[${PROG}] Error at line $LINENO: $BASH_COMMAND" >&2' ERR

# ---------- script dir & optional utilities ----------------------------------
# shellcheck disable=SC1091
if [[ -f "$SCRIPT_DIR/version-utils.sh" ]]; then
    # Expected to provide: init_colors, die, split_semver, is_uint
    # shellcheck source=/dev/null
    # shellcheck disable=SC1091
    source "$SCRIPT_DIR/version-utils.sh"
fi

# ---------- fallbacks (standalone) -------------------------------------------
: "${NO_COLOR:=false}"

has_cmd() { command -v "$1" >/dev/null 2>&1; }

if ! has_cmd die; then
    die() { printf 'Error: %s\n' "$*" >&2; exit 1; }
fi

if ! has_cmd init_colors; then
    init_colors() { :; }
fi

# Provide split_semver if not sourced from version-utils.sh
if ! has_cmd split_semver 2>/dev/null; then
    split_semver() {
        local v="${1-}"
        [[ "$v" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "Invalid version format: $v (expected MAJOR.MINOR.PATCH)"
        IFS='.' read -r maj min pat <<<"$v"
        printf '%s\n%s\n%s' "$maj" "$min" "$pat"
    }
fi

# Provide is_uint if not sourced from version-utils.sh
if ! has_cmd is_uint 2>/dev/null; then
    is_uint() { [[ "${1-}" =~ ^[0-9]+$ ]]; }
fi

# ---------- tiny helpers ------------------------------------------------------
need_val() { [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "$1 requires a value"; }
# json_escape() function is now sourced from version-utils.sh

# ---------- defaults (env-overridable) ---------------------------------------
: "${VERSION_PATCH_LIMIT:=1000}"
: "${VERSION_MINOR_LIMIT:=1000}"
: "${VERSION_PATCH_DELTA:=1}"
: "${VERSION_MINOR_DELTA:=5}"
: "${VERSION_MAJOR_DELTA:=10}"
: "${PRESERVE_PATCH_ON_MINOR:=false}"   # keep patch when minor bump
: "${PRESERVE_LOWER_ON_MAJOR:=false}"   # keep minor+patch when major bump

check_env_sanity() {
    local k v
    for k in VERSION_PATCH_LIMIT VERSION_MINOR_LIMIT VERSION_PATCH_DELTA VERSION_MINOR_DELTA VERSION_MAJOR_DELTA; do
        v="${!k}"
        is_uint "$v" || die "Environment $k must be an unsigned integer (got '$v')"
    done
    case "${PRESERVE_PATCH_ON_MINOR,,}" in true|false) ;; *) die "PRESERVE_PATCH_ON_MINOR must be true|false";; esac
    case "${PRESERVE_LOWER_ON_MAJOR,,}" in true|false) ;; *) die "PRESERVE_LOWER_ON_MAJOR must be true|false";; esac
}

# ---------- semantic analyzer integration ------------------------------------
find_semantic_analyzer() {
    local original_project_root="${1-}" current_dir="${2-}" explicit="${3-}"
    if [[ -n "$explicit" && -x "$explicit" ]]; then printf '%s' "$explicit"; return; fi
    local c
    for c in \
        "$original_project_root/bin/semantic-version-analyzer.sh" \
        "$current_dir/bin/semantic-version-analyzer.sh" \
        "$SCRIPT_DIR/semantic-version-analyzer.sh"
    do
        [[ -x "$c" ]] && { printf '%s' "$c"; return; }
    done
    if has_cmd semantic-version-analyzer; then
        command -v semantic-version-analyzer
        return
    fi
    printf ''  # not found
}

json_number_or_empty() {
    local key="$1" json="$2"
    if has_cmd jq; then
        jq -r ".. | objects | .\"$key\"? // empty" <<<"$json" | awk 'NR==1'
    else
        grep -Eo "\"$key\"[[:space:]]*:[[:space:]]*[0-9]+" <<<"$json" \
            | head -1 | awk -F: '{gsub(/[[:space:]]/,"",$2); print $2}'
    fi
}

get_semantic_delta() {
    local analyzer="$1" bump="$2" repo_root="${3-}" cwd_base="${4-}"
    [[ -x "$analyzer" ]] || { printf ''; return 1; }
    local out args=(--json)
    [[ -n "$repo_root" ]] && args+=(--repo-root "$repo_root")
    if [[ -d "$cwd_base" ]]; then
        out="$( (cd "$cwd_base" && "$analyzer" "${args[@]}") 2>/dev/null || true)"
    else
        out="$("$analyzer" "${args[@]}" 2>/dev/null || true)"
    fi
    [[ -n "$out" ]] || { printf ''; return 1; }
    local key
    case "$bump" in
        patch) key=loc_delta.patch_delta ;;
        minor) key=loc_delta.minor_delta ;;
        major) key=loc_delta.major_delta ;;
        *)     printf ''; return 1 ;;
    esac
    local v
    v="$(json_number_or_empty "$key" "$out" | head -1 || true)"
    [[ -n "$v" && "$v" =~ ^[0-9]+$ ]] && { printf '%s' "$v"; return 0; }
    printf ''; return 1
}

get_analysis_explanation() {
    local analyzer="$1" cwd_base="${2-}"
    [[ -x "$analyzer" ]] || { printf ''; return 0; }
    local out
    if [[ -d "$cwd_base" ]]; then
        out="$( (cd "$cwd_base" && "$analyzer" --verbose) 2>/dev/null || true)"
    else
        out="$("$analyzer" --verbose 2>/dev/null || true)"
    fi
    # Try to extract reason line, but don't fail if not found
    grep -E '^Reason:' <<<"$out" | head -1 | sed -E 's/^Reason:[[:space:]]*//' || printf ''
}

# ---------- version checks ----------------------------------------------------
# Use validate_version_format from version-utils.sh if available, otherwise provide fallback
if ! has_cmd validate_version_format 2>/dev/null; then
    validate_version_format() {
        local v="$1"
        [[ "$v" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "Invalid version format: $v (expected MAJOR.MINOR.PATCH)"
    }
fi



# ---------- config & output ---------------------------------------------------
load_version_config() {
    local file="${1-}"
    [[ -z "$file" ]] && return 0
    [[ -f "$file" ]] || die "Config file not found: $file"
    # shellcheck source=/dev/null
    source "$file"
}

emit_json() {
    local new="$1" old="$2" type="$3" delta="$4" source="$5" reason="$6"
    printf '{'
    printf '"old":"%s",'  "$(json_escape "$old")"
    printf '"new":"%s",'  "$(json_escape "$new")"
    printf '"bump_type":"%s",' "$(json_escape "$type")"
    printf '"delta":%s,'  "${delta:-0}"
    printf '"delta_source":"%s",' "$(json_escape "${source:-default}")"
    printf '"reason":"%s"' "$(json_escape "${reason-}")"
    printf '}\n'
}

emit_machine() {
    local new="$1" old="$2" type="$3" delta="$4" source="$5"
    printf 'OLD=%s\nNEW=%s\nTYPE=%s\nDELTA=%s\nSOURCE=%s\n' \
        "$old" "$new" "$type" "${delta:-0}" "${source:-default}"
}



# --- Standalone usage --------------------------------------------------------
usage() {
    cat <<'EOF'
Usage: version-calculator-loc [options]

Options:
  --current-version <ver>    Current version (MAJOR.MINOR.PATCH) [required]
  --bump-type <type>         One of: major | minor | patch       [required unless --set]
  --set <version>            Set version directly (X.Y.Z format) [mutually exclusive with --bump-type]
  --original-project-root <p>Directory to run analyzer from
  --repo-root <p>            Repository root to pass to analyzer
  --config <file>            Source additional configuration (bash)
  --analyzer-path <file>     Explicit path to semantic-version-analyzer
  --json                     Output JSON {old,new,bump_type,delta,delta_source,reason}
  --machine                  Output machine-readable key=value lines
  --verbose                  Print a short explanation to stderr
  --help, -h                 Show this help

Environment:
  VERSION_PATCH_LIMIT   [default: 1000]
  VERSION_MINOR_LIMIT   [default: 1000]
  VERSION_PATCH_DELTA   [default: 1]
  VERSION_MINOR_DELTA   [default: 5]
  VERSION_MAJOR_DELTA   [default: 10]
  PRESERVE_PATCH_ON_MINOR [default: false]
  PRESERVE_LOWER_ON_MAJOR [default: false]
  NO_COLOR               [default: false]

Examples:
  version-calculator-loc --current-version 1.2.3 --bump-type patch
  version-calculator-loc --current-version 1.2.3 --bump-type minor --repo-root ~/proj
  version-calculator-loc --current-version 9.99.99 --bump-type patch VERSION_MINOR_LIMIT=100
  version-calculator-loc --current-version 1.2.3 --set 2.0.0
EOF
}

main() {
    # Initialize colors
    init_colors "${NO_COLOR:-false}"
    check_env_sanity
    
    # Parse command line arguments
    local current_version="" bump_type="" set_version=""
    local original_project_root="" repo_root="" config_file="" analyzer_path_override=""
    local out_json=false out_machine=false verbose=false
    
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --current-version)
                need_val "$1" "${2-}"; current_version="$2"; shift 2 ;;
            --bump-type)
                need_val "$1" "${2-}"; bump_type="$2"; shift 2 ;;
            --set)
                need_val "$1" "${2-}"; set_version="$2"; shift 2 ;;
            --original-project-root)
                need_val "$1" "${2-}"; original_project_root="$2"; shift 2 ;;
            --repo-root)
                need_val "$1" "${2-}"; repo_root="$2"; shift 2 ;;
            --config)
                need_val "$1" "${2-}"; config_file="$2"; shift 2 ;;
            --analyzer-path)
                need_val "$1" "${2-}"; analyzer_path_override="$2"; shift 2 ;;
            --json)
                out_json=true; shift ;;
            --machine)
                out_machine=true; shift ;;
            --verbose)
                verbose=true; shift ;;
            --help|-h)
                usage; exit 0 ;;
            *)
                die "Unknown option: $1 (use --help)" ;;
        esac
    done
    
    # Load optional config (may override env defaults)
    [[ -n "$config_file" ]] && load_version_config "$config_file"
    
    # Auto-detect repo root if not provided and git available
    if [[ -z "$repo_root" ]] && has_cmd git; then
        repo_root="$(git rev-parse --show-toplevel 2>/dev/null || true)"
    fi
    
    # Validate required arguments
    [[ -n "$current_version" ]] || die "--current-version is required"
    
    # Must specify either a bump type or --set
    if [[ -n "$set_version" && -n "$bump_type" ]]; then
        die "Cannot specify both --set and --bump-type"
    fi
    if [[ -z "$set_version" && -z "$bump_type" ]]; then
        die "Specify --set <X.Y.Z> or --bump-type {major|minor|patch}"
    fi
    
    # Handle --set case
    if [[ -n "$set_version" ]]; then
        # Validate set version format
        validate_version_format "$set_version"
        
        if $verbose; then
            printf 'Set: direct  Old: %s  New: %s\n' "$current_version" "$set_version" >&2
        fi
        
        if $out_json; then
            emit_json "$set_version" "$current_version" "set" "0" "direct" "Version set directly"
        elif $out_machine; then
            emit_machine "$set_version" "$current_version" "set" "0" "direct"
        else
            printf '%s\n' "$set_version"
        fi
        exit 0
    fi
    
    # Validate bump type
    case "$bump_type" in major|minor|patch) ;; *) die "Invalid --bump-type: $bump_type";; esac
    
    # Analyzer lookup and delta resolution
    local analyzer_path delta delta_source="default" reason=""
    analyzer_path="$(find_semantic_analyzer "${original_project_root:-$repo_root}" "$(pwd)" "${analyzer_path_override:-}")"
    
    if [[ -n "$analyzer_path" ]]; then
        delta="$(get_semantic_delta "$analyzer_path" "$bump_type" "$repo_root" "${original_project_root:-$repo_root}" || true)"
        if [[ -n "$delta" && "$delta" =~ ^[0-9]+$ ]]; then
            delta_source="analyzer"
            reason="$(get_analysis_explanation "$analyzer_path" "${original_project_root:-$repo_root}")"
        fi
    fi
    
    # Fallback to defaults if analyzer not available or returned nothing
    if [[ -z "${delta-}" ]]; then
        case "$bump_type" in
            patch) delta="$VERSION_PATCH_DELTA" ;;
            minor) delta="$VERSION_MINOR_DELTA" ;;
            major) delta="$VERSION_MAJOR_DELTA" ;;
        esac
    fi
    
    # Calculate new version using version-calculator.sh
    local new_version
    new_version="$("$SCRIPT_DIR/version-calculator.sh" \
        --current-version "$current_version" \
        --bump-type "$bump_type" \
        --loc 0 \
        --bonus "${delta:-0}" \
        --quiet)"
    
    if $verbose; then
        {
            printf 'Bump: %s  Old: %s  New: %s  Delta: %s  Source: %s\n' \
                "$bump_type" "$current_version" "$new_version" "$delta" "$delta_source"
            [[ -n "$analyzer_path" ]] && printf 'Analyzer: %s\n' "$analyzer_path"
            [[ -n "$reason" ]] && printf 'Reason: %s\n' "$reason"
        } >&2
    fi
    
    if $out_json; then
        emit_json "$new_version" "$current_version" "$bump_type" "$delta" "$delta_source" "$reason"
    elif $out_machine; then
        emit_machine "$new_version" "$current_version" "$bump_type" "$delta" "$delta_source"
    else
        printf '%s\n' "$new_version"
    fi
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 