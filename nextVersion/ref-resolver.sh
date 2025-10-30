#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Reference resolver for next-version
# Resolves git references and calculates commit counts

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C

# Source common utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# Prevent any pager and avoid unnecessary repo locks for better performance.
export GIT_PAGER=cat PAGER=cat GIT_OPTIONAL_LOCKS=0

show_help() {
    cat << 'EOF'
Reference Resolver

Usage: reference-resolver [options]

Options:
  --since <tag>            Analyze changes since specific tag (default: last tag)
  --since-tag <tag>        Alias for --since
  --since-commit <hash>    Analyze changes since specific commit
  --since-date <date>      Analyze changes since specific date (YYYY-MM-DD)
  --base <ref>             Set base reference for comparison (default: auto-detected)
  --target <ref>           Set target reference for comparison (default: HEAD)
  --repo-root <path>       Set repository root directory
  --tag-match <glob>       Glob for last-tag detection (default: '*'), e.g. 'v*'
  --first-parent           Count commits using --first-parent
  --no-merge-base          Disable automatic merge-base adjustment for disjoint branches
  --print-base             Output the chosen base reference SHA only
  --machine                Output machine-readable key=value format
  --json                   Output machine-readable JSON
  --help, -h               Show this help

Examples:
  reference-resolver --since v1.1.0
  reference-resolver --since-date 2025-01-01
  reference-resolver --base HEAD~5 --target HEAD
  reference-resolver --base v1.0.0 --target v1.1.0 --print-base
EOF
}

# --- args ---
SINCE_TAG=""
SINCE_COMMIT=""
SINCE_DATE=""
BASE_REF=""
TARGET_REF="HEAD"
REPO_ROOT=""
TAG_MATCH="*"
FIRST_PARENT=false
NO_MERGE_BASE=false
PRINT_BASE=false
MACHINE_OUTPUT=false
JSON_OUTPUT=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --since|--since-tag)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || { printf 'Error: --since requires a value\n' >&2; exit 1; }
            SINCE_TAG=$2; shift 2;;
        --since-commit)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || { printf 'Error: --since-commit requires a value\n' >&2; exit 1; }
            SINCE_COMMIT=$2; shift 2;;
        --since-date)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || { printf 'Error: --since-date requires a value\n' >&2; exit 1; }
            # Validate date format (YYYY-MM-DD)
            [[ "$2" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]] || { printf 'Error: --since-date requires YYYY-MM-DD format\n' >&2; exit 1; }
            IFS='-' read -r _ m d <<< "$2"
            ((m>=1 && m<=12)) || { printf 'Error: Invalid month in date %s (01-12)\n' "$2" >&2; exit 1; }
            ((d>=1 && d<=31)) ||  { printf 'Error: Invalid day in date %s (01-31)\n' "$2" >&2; exit 1; }
            SINCE_DATE=$2; shift 2;;
        --base)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || { printf 'Error: --base requires a value\n' >&2; exit 1; }
            BASE_REF=$2; shift 2;;
        --target)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || { printf 'Error: --target requires a value\n' >&2; exit 1; }
            TARGET_REF=$2; shift 2;;
        --repo-root)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || { printf 'Error: --repo-root requires a value\n' >&2; exit 1; }
            REPO_ROOT=$2; shift 2;;
        --tag-match)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || { printf 'Error: --tag-match requires a value\n' >&2; exit 1; }
            TAG_MATCH=$2; shift 2;;
        --first-parent) FIRST_PARENT=true; shift;;
        --no-merge-base) NO_MERGE_BASE=true; shift;;
        --print-base) PRINT_BASE=true; shift;;
        --machine) MACHINE_OUTPUT=true; shift;;
        --json) JSON_OUTPUT=true; shift;;
        --help|-h) show_help; exit 0;;
        *) printf 'Error: Unknown option: %s\n' "$1" >&2; show_help; exit 1;;
    esac
done

# --- helpers ---
# die() function is now sourced from version-utils.sh
# warn() function is now sourced from version-utils.sh
# info() function is now sourced from version-utils.sh

# json_escape() function is now sourced from version-utils.sh

# Check git command
if ! command -v git >/dev/null 2>&1; then
    die "git command not found"
fi

# Change to repo root if specified
if [[ -n "$REPO_ROOT" ]]; then
    cd "$REPO_ROOT" || die "Cannot cd to repo root: $REPO_ROOT"
    git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "Not in a git repository at $REPO_ROOT"
fi

# Return 0 if repo has at least one commit; otherwise 1 (unborn HEAD)
repo_has_commits() {
    git -c color.ui=false rev-parse -q --verify "HEAD^{commit}" >/dev/null 2>&1
}

# Resolve a ref (tag/branch/sha) to a commit SHA (40-hex). Echo empty on failure.
resolve_sha() {
    local ref=$1
    git -c color.ui=false rev-parse -q --verify "${ref}^{commit}" 2>/dev/null || true
}

# verify_ref() function is now sourced from version-utils.sh

# Count commits between base..target; echoes integer.
count_commits() {
    local base=$1 target=$2
    local args=(--count)
    $FIRST_PARENT && args=(--first-parent "${args[@]}")
    git -c color.ui=false rev-list "${args[@]}" "${base}..${target}" 2>/dev/null || printf '0'
}

# Get last tag matching pattern; echoes tag name or empty.
get_last_tag() {
    # Prefer nearest annotated/lightweight tag reachable from HEAD
    git -c color.ui=false describe --tags --abbrev=0 --match "$TAG_MATCH" 2>/dev/null || true
}

# --- output functions ---
emit_json() {
    local base_ref=$1 base_sha=$2 base_type=$3 req_base_sha=$4 target_ref=$5 target_sha=$6 commit_count=$7 has_commits=$8 \
          empty_repo=$9 single_commit=${10} effective_base_sha=${11-}

    printf '{\n'
    printf '  "base_ref": "%s",\n' "$(json_escape "$base_ref")"
    printf '  "base_sha": "%s",\n' "$base_sha"
    printf '  "base_ref_type": "%s",\n' "$base_type"
    printf '  "requested_base_sha": "%s",\n' "$req_base_sha"
    printf '  "effective_base_sha": "%s",\n' "${effective_base_sha:-$base_sha}"
    printf '  "target_ref": "%s",\n' "$(json_escape "$target_ref")"
    printf '  "target_sha": "%s",\n' "$target_sha"
    printf '  "commit_count": %s,\n' "$commit_count"
    printf '  "has_commits": %s,\n' "$has_commits"
    printf '  "empty_repo": %s,\n' "$empty_repo"
    printf '  "single_commit_repo": %s\n' "$single_commit"
    printf '}\n'
}

emit_machine() {
    local base_ref=$1 base_sha=$2 base_type=$3 req_base_sha=$4 target_ref=$5 target_sha=$6 commit_count=$7 has_commits=$8 \
          empty_repo=$9 single_commit=${10} effective_base_sha=${11-}

    printf 'BASE_REF=%s\n' "$base_ref"
    printf 'BASE_SHA=%s\n' "$base_sha"
    printf 'BASE_REF_TYPE=%s\n' "$base_type"
    printf 'REQUESTED_BASE_SHA=%s\n' "$req_base_sha"
    printf 'EFFECTIVE_BASE_SHA=%s\n' "${effective_base_sha:-$base_sha}"
    printf 'TARGET_REF=%s\n' "$target_ref"
    printf 'TARGET_SHA=%s\n' "$target_sha"
    printf 'COMMIT_COUNT=%s\n' "$commit_count"
    printf 'HAS_COMMITS=%s\n' "$has_commits"
    printf 'EMPTY_REPO=%s\n' "$empty_repo"
    printf 'SINGLE_COMMIT_REPO=%s\n' "$single_commit"
}

emit_human() {
    local base_ref=$1 base_sha=$2 base_type=$3 target_ref=$4 target_sha=$5 commit_count=$6 has_commits=$7 effective_base_sha=${8-}
    printf '=== Reference Resolution ===\n'
    printf 'Base reference: %s (%s)\n' "$base_ref" "$base_type"
    [[ -n "${effective_base_sha-}" && "$effective_base_sha" != "$base_sha" ]] && \
        printf 'Effective merge-base: %s\n' "$effective_base_sha"
    printf 'Base SHA:   %s\n' "$base_sha"
    printf 'Target ref: %s\n' "$target_ref"
    printf 'Target SHA: %s\n' "$target_sha"
    printf 'Commit count: %s\n' "$commit_count"
    [[ "$has_commits" == "true" ]] && printf 'Status: Commits found in range\n' || printf 'Status: No commits found in range\n'
}

# Determine the base for comparison with better error handling
get_base_reference() {
    # Handle explicit base/target references
    if [[ -n "$BASE_REF" ]]; then
        printf '%s|explicit_base' "$BASE_REF"; return
    fi

    if [[ -n "$SINCE_COMMIT" ]]; then
        printf '%s|commit' "$SINCE_COMMIT"; return
    elif [[ -n "$SINCE_TAG" ]]; then
        printf '%s|tag' "$SINCE_TAG"; return
    elif [[ -n "$SINCE_DATE" ]]; then
        # Find the latest commit before the specified date
        local ref
        ref=$(git -c color.ui=false rev-list -1 --before="$SINCE_DATE 23:59:59" HEAD 2>/dev/null || true)
        if [[ -n "$ref" ]]; then
            printf '%s|date' "$ref"
        else
            warn "No commits found before $SINCE_DATE"
            local first_commit
            first_commit=$(git -c color.ui=false rev-list --max-parents=0 HEAD 2>/dev/null || true)
            if [[ -n "$first_commit" ]]; then
                printf '%s|first' "$first_commit"
            else
                # Empty repository - no commits yet
                printf 'EMPTY|empty'
            fi
        fi
        return
    fi

    # Default to last tag, fallback to HEAD~1 if no tags exist
    local last_tag
    last_tag=$(get_last_tag)
    if [[ -n "$last_tag" ]]; then
        printf '%s|last_tag' "$last_tag"
    else
        # If no tags exist, use HEAD~1 instead of first commit
        local parent_commit
        parent_commit=$(git -c color.ui=false rev-parse -q --verify HEAD~1 2>/dev/null || true)
        if [[ -n "$parent_commit" ]]; then
            printf '%s|parent' "$parent_commit"
        else
            # Only use first commit if HEAD~1 doesn't exist (single-commit repo)
            local first_commit
            first_commit=$(git -c color.ui=false rev-list --max-parents=0 HEAD 2>/dev/null || true)
            if [[ -n "$first_commit" ]]; then
                printf '%s|first' "$first_commit"
            else
                # Empty repository - no commits yet
                printf 'EMPTY|empty'
            fi
        fi
    fi
}

# Main execution
main() {
    # Empty repo handling (unborn HEAD)
    if ! repo_has_commits; then
        # No commits at all
        if $PRINT_BASE; then
            printf 'EMPTY\n'
            exit 0
        fi
        if $JSON_OUTPUT; then
            emit_json "EMPTY" "EMPTY" "empty" "EMPTY" "$TARGET_REF" "" 0 false true false ""
        elif $MACHINE_OUTPUT; then
            emit_machine "EMPTY" "EMPTY" "empty" "EMPTY" "$TARGET_REF" "" 0 false true false ""
        else
            printf '=== Reference Resolution ===\n'
            printf 'Empty repository - no commits yet\n'
            printf 'Base reference: EMPTY (empty)\n'
            printf 'Target reference: %s\n' "$TARGET_REF"
        fi
        exit 0
    fi

    # Resolve target SHA
    local target_sha
    target_sha=$(resolve_sha "$TARGET_REF")
    [[ -n "$target_sha" ]] || die "Invalid target reference: $TARGET_REF"

    # Get base reference selection
    local base_ref_info
    base_ref_info=$(get_base_reference)
    local base_ref ref_type
    IFS='|' read -r base_ref ref_type <<< "$base_ref_info"

    # Resolve base to SHA (unless special EMPTY)
    local requested_base_sha=""
    local base_sha=""
    if [[ "$base_ref" == "EMPTY" ]]; then
        # repo_has_commits already true, so this should not happen here, but keep for safety
        if $PRINT_BASE; then printf 'EMPTY\n'; exit 0; fi
        die "Internal: EMPTY base in non-empty repository"
    else
        requested_base_sha=$(resolve_sha "$base_ref")
        [[ -n "$requested_base_sha" ]] || die "Invalid base reference: $base_ref"
        base_sha=$requested_base_sha
    fi

    # Check and normalize disjoint branches with merge-base
    local effective_base_sha
    effective_base_sha=$(git -c color.ui=false merge-base "$base_sha" "$target_sha" 2>/dev/null || true)
    if [[ -z "$effective_base_sha" ]]; then
        # Check if this is a single-commit repository
        local total_count
        total_count=$(git -c color.ui=false rev-list --count HEAD 2>/dev/null || printf '0')
        if [[ "$total_count" = "1" ]]; then
            # Single commit repository - no changes to analyze
            if $PRINT_BASE; then
                printf '%s\n' "$base_sha"
                exit 0
            fi
            if $JSON_OUTPUT; then
                emit_json "$base_ref" "$base_sha" "$ref_type" "$requested_base_sha" "$TARGET_REF" "$target_sha" 0 false false true ""
            elif $MACHINE_OUTPUT; then
                emit_machine "$base_ref" "$base_sha" "$ref_type" "$requested_base_sha" "$TARGET_REF" "$target_sha" 0 false false true ""
            else
                printf '=== Reference Resolution ===\n'
                printf 'Single commit repository - no changes to analyze\n'
                printf 'Base reference: %s (%s)\n' "$base_ref" "$ref_type"
                printf 'Base SHA: %s\n' "$base_sha"
                printf 'Target reference: %s\n' "$TARGET_REF"
                printf 'Target SHA: %s\n' "$target_sha"
            fi
            exit 0
        else
            die "No common ancestor found between $base_ref and $TARGET_REF"
        fi
    fi

    # Optionally replace base with merge-base
    if [[ "$effective_base_sha" != "$base_sha" && "$NO_MERGE_BASE" != "true" ]]; then
        info "Using merge-base $effective_base_sha instead of requested $base_sha"
        base_sha="$effective_base_sha"
        ref_type="merge_base"
    fi

    # Handle --print-base option
    if $PRINT_BASE; then
        printf '%s\n' "$base_sha"
        exit 0
    fi

    # Count commits in range
    local commit_count has_commits=false
    commit_count=$(count_commits "$base_sha" "$target_sha")
    [[ "$commit_count" != "0" ]] && has_commits=true

    # Output results
    if $JSON_OUTPUT; then
        emit_json "$base_ref" "$base_sha" "$ref_type" "$requested_base_sha" "$TARGET_REF" "$target_sha" "$commit_count" "$has_commits" false false "$effective_base_sha"
    elif $MACHINE_OUTPUT; then
        emit_machine "$base_ref" "$base_sha" "$ref_type" "$requested_base_sha" "$TARGET_REF" "$target_sha" "$commit_count" "$has_commits" false false "$effective_base_sha"
    else
        emit_human "$base_ref" "$base_sha" "$ref_type" "$TARGET_REF" "$target_sha" "$commit_count" "$has_commits" "$effective_base_sha"
    fi
}

# Run main function
main "$@" 