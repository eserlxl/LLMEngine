#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# File change analyzer for next-version
# Analyzes file changes in git diffs

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C

# Source common utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# Bash features used by this script (for trimming globs safely)
shopt -s extglob

# PROG="${0##*/}"  # Unused variable - commented out to fix shellcheck warning

show_help() {
    cat << 'EOF'
File Change Analyzer

Usage: file-change-analyzer [options]

Options:
  --base <ref>             Base reference for comparison              (required)
  --target <ref>           Target reference for comparison (default: HEAD)
  --repo-root <path>       Set repository root directory
  --only-paths <globs>     Include only comma-separated path globs (pathspecs)
  --exclude-paths <globs>  Exclude comma-separated path globs (pathspecs)
  --ignore-whitespace      Ignore whitespace changes in diff analysis
  --machine                Output machine-readable key=value format
  --json                   Output machine-readable JSON
  --quiet                  Suppress warnings/info (non-error output)
  --exit-code              Exit 1 if changes exist, 0 if none (errors still !=0)
  --help, -h               Show this help

Notes:
  * --machine and --json are mutually exclusive.
  * Path globs are passed as git pathspecs after a "--" separator.

Examples:
  file-change-analyzer --base v1.0.0 --target HEAD
  file-change-analyzer --base HEAD~5 --target HEAD --machine
  file-change-analyzer --base v1.0.0 --target v1.1.0 --json
  file-change-analyzer --base v1.0.0 --exclude-paths "*.md,docs/**" --exit-code
EOF
}

# --- Error handling functions ---
# die() function is now sourced from version-utils.sh
warn() { [[ "${QUIET:-false}" == "true" ]] || printf 'Warning: %s\n' "$*\n" >&2; }
err()  { printf 'Error: %s\n' "$*\n" >&2; }

# --- Parse arguments ----------------------------------------------------------
BASE_REF=""
TARGET_REF="HEAD"
REPO_ROOT=""
ONLY_PATHS=""
EXCLUDE_PATHS=""
IGNORE_WHITESPACE=false
MACHINE_OUTPUT=false
JSON_OUTPUT=false
QUIET=false
EXIT_CODE_ON_CHANGE=false

needval() { [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "$1 requires a value"; }

while [[ $# -gt 0 ]]; do
    case $1 in
        --base)
            needval "$1" "$2"; BASE_REF="$2"; shift 2;;
        --target)
            needval "$1" "$2"; TARGET_REF="$2"; shift 2;;
        --repo-root)
            needval "$1" "$2"; REPO_ROOT="$2"; shift 2;;
        --only-paths)
            needval "$1" "$2"; ONLY_PATHS="$2"; shift 2;;
        --exclude-paths)
            needval "$1" "$2"; EXCLUDE_PATHS="$2"; shift 2;;
        --ignore-whitespace) IGNORE_WHITESPACE=true; shift;;
        --machine) MACHINE_OUTPUT=true; shift;;
        --json) JSON_OUTPUT=true; shift;;
        --quiet) QUIET=true; shift;;
        --exit-code) EXIT_CODE_ON_CHANGE=true; shift;;
        --help|-h) show_help; exit 0;;
        *) err "Unknown option: $1"; show_help; exit 2;;
    esac
done

# --- Validation & setup -------------------------------------------------------
# Validate required arguments
[[ -n "$BASE_REF" ]] || { err "--base is required"; show_help; exit 2; }

# Check git command
command -v git >/dev/null 2>&1 || die "git command not found"

# Validate output format options
if [[ "$MACHINE_OUTPUT" == true && "$JSON_OUTPUT" == true ]]; then
    die "--json and --machine cannot be used together"
fi

# Change to repo root if specified
if [[ -n "$REPO_ROOT" ]]; then
    cd "$REPO_ROOT" || die "Cannot cd to $REPO_ROOT"
    git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "Not a git repo: $REPO_ROOT"
fi

# Verify git reference exists
verify_ref() {
    local ref="$1"
    git rev-parse --verify --quiet "${ref}^{commit}" >/dev/null || die "Invalid reference: $ref"
}
verify_ref "$BASE_REF"
verify_ref "$TARGET_REF"

# --- Pathspec handling ---
trim_spaces() { local s="$1"; s="${s##+([[:space:]])}"; s="${s%%+([[:space:]])}"; printf '%s' "$s"; }

PATH_ARGS=()
if [[ -n "$ONLY_PATHS" ]]; then
    PATH_ARGS=(--)
    IFS=',' read -r -a _inc <<< "$ONLY_PATHS"
    for g in "${_inc[@]}"; do
        g="$(trim_spaces "$g")"
        [[ -n "$g" ]] && PATH_ARGS+=("$g")
    done
fi

# negative pathspecs (exclude) must come before "--"
NEG_PATH_ARGS=()
if [[ -n "$EXCLUDE_PATHS" ]]; then
    IFS=',' read -r -a _exc <<< "$EXCLUDE_PATHS"
    for g in "${_exc[@]}"; do
        g="$(trim_spaces "$g")"
        [[ -n "$g" ]] && NEG_PATH_ARGS+=(":!$g")
    done
fi

# --- Classification -----------------------------------------------------------
# Returns: 30=source, 10=test, 20=doc, 0=other/ignored
classify_path() {
    local path="$1"
    local path_lower
    path_lower=$(printf '%s' "$path" | tr '[:upper:]' '[:lower:]')

    # Ignore build/third-party/binary artifacts (nested-aware)
    if [[ "$path" =~ (^|/)(build|dist|out|third[_-]?party|vendor|\.git|node_modules|target|bin|obj)(/|$) ]] || \
       [[ "$path" =~ \.(lock|exe|dll|so|dylib|a|jar|war|ear|zip|tar|gz|bz2|xz|7z|rar|png|jpe?g|gif|bmp|ico|pdf)$ ]]; then
        printf '0'; return
    fi

    # Tests first to avoid misclassifying "src/test" as source
    if [[ "$path" =~ (^|/)(test|tests|unittests|it|e2e)(/|$) ]] || \
       [[ "$path" =~ (_test|\.test|\.spec)\.(c|cc|cpp|cxx|py|js|ts|sh)$ ]]; then
        printf '10'; return
    fi

    # Source files (common languages + C/C++ + build scripts)
    if [[ "$path" =~ (^|/)(src|source|app|lib|include)(/|$) ]] || \
       [[ "$path" =~ \.(c|cc|cpp|cxx|h|hh|hpp|inl|go|rs|java|cs|m|mm|swift|kt|ts|tsx|js|jsx|sh|py|rb|php|pl|lua|sql|cmake|yml|yaml)$ ]] || \
       [[ "$path" =~ (^|/)(CMakeLists\.txt|Makefile|makefile|GNUmakefile)$ ]]; then
        printf '30'; return
    fi

    # Documentation
    if [[ "$path_lower" =~ (^|/)(doc|docs|documentation|examples)(/|$) ]] || \
       [[ "$path_lower" =~ (^|/)(readme|changelog|contributing|license|copying|authors|install|news|history) ]] || \
       [[ "$path_lower" =~ \.(md|markdown|mkd|rst|adoc|txt)$ ]]; then
        printf '20'; return
    fi

    printf '0'
}

# --- Helpers -----------------------------------------------------------------
# Binary-safe numstat summation (treats '-' as 0)
sum_numstat() {
    local total_ins=0 total_del=0
    # shellcheck disable=SC2068  # intentional array expansion to pass full cmd
    while IFS=$'\t' read -r ins del _; do
        [[ $ins =~ ^[0-9]+$ ]] || ins=0
        [[ $del =~ ^[0-9]+$ ]] || del=0
        total_ins=$((total_ins + ins))
        total_del=$((total_del + del))
    done < <("$@")
    printf '%s\t%s' "$total_ins" "$total_del"
}

print_results() {
    local added="$1" modified="$2" deleted="$3" ns="$4" nt="$5" nd="$6" ins="$7" dels="$8"
    local diff_size=$((ins + dels))

    if [[ "$JSON_OUTPUT" == true ]]; then
        printf '{\n'
        printf '  "added_files": %s,\n'        "$added"
        printf '  "modified_files": %s,\n'     "$modified"
        printf '  "deleted_files": %s,\n'      "$deleted"
        printf '  "new_source_files": %s,\n'   "$ns"
        printf '  "new_test_files": %s,\n'     "$nt"
        printf '  "new_doc_files": %s,\n'      "$nd"
        printf '  "insertions": %s,\n'         "$ins"
        printf '  "deletions": %s,\n'          "$dels"
        printf '  "diff_size": %s\n'           "$diff_size"
        printf '}\n'
        return
    fi

    if [[ "$MACHINE_OUTPUT" == true ]]; then
        printf 'ADDED_FILES=%s\n'        "$added"
        printf 'MODIFIED_FILES=%s\n'     "$modified"
        printf 'DELETED_FILES=%s\n'      "$deleted"
        printf 'NEW_SOURCE_FILES=%s\n'   "$ns"
        printf 'NEW_TEST_FILES=%s\n'     "$nt"
        printf 'NEW_DOC_FILES=%s\n'      "$nd"
        printf 'INSERTIONS=%s\n'         "$ins"
        printf 'DELETIONS=%s\n'          "$dels"
        printf 'DIFF_SIZE=%s\n'          "$diff_size"
        return
    fi

    printf '=== File Change Analysis ===\n'
    printf 'Base reference:   %s\n' "$BASE_REF"
    printf 'Target reference: %s\n' "$TARGET_REF"
    printf '\nFile Changes:\n'
    printf '  Added files:    %s\n' "$added"
    printf '  Modified files: %s\n' "$modified"
    printf '  Deleted files:  %s\n' "$deleted"
    printf '\nNew Content:\n'
    printf '  New source files: %s\n' "$ns"
    printf '  New test files:   %s\n' "$nt"
    printf '  New doc files:    %s\n' "$nd"
    printf '\nChange Magnitude:\n'
    printf '  Insertions: %s\n' "$ins"
    printf '  Deletions:  %s\n' "$dels"
    printf '  Diff size:  %s lines\n' "$((ins + dels))"
}

print_empty_results() {
    local note="$1"
    [[ -n "$note" ]] && warn "$note"
    print_results 0 0 0 0 0 0 0 0
}

# --- Core --------------------------------------------------------------------
analyze_file_changes() {
    local base_ref="$1" target_ref="$2"
    
    # Initialize counters
    local added=0 modified=0 deleted=0
    local new_src=0 new_tst=0 new_doc=0
    local ins=0 dels=0

    # shared git args
    local -a CFG=(-c color.ui=false -c core.quotepath=false)
    local -a DIFF_OPTS=(-M -C) # detect renames/copies
    [[ "$IGNORE_WHITESPACE" == true ]] && DIFF_OPTS+=(-w)

    # fast path: any changes?
    local git_args=("${CFG[@]}" diff "${DIFF_OPTS[@]}" --quiet "${base_ref}..${target_ref}")
    [[ ${#NEG_PATH_ARGS[@]} -gt 0 ]] && git_args+=("${NEG_PATH_ARGS[@]}")
    [[ ${#PATH_ARGS[@]} -gt 0 ]] && git_args+=(-- "${PATH_ARGS[@]}")
    
    if git "${git_args[@]}" 2>/dev/null; then
        local msg="No changes detected between ${base_ref} and ${target_ref}"
        [[ "$IGNORE_WHITESPACE" == true ]] && msg+=" (ignoring whitespace)"
        print_empty_results "$msg"
        return 0
    fi

    # name-status, NUL-delimited
    local git_cmd_args=("${CFG[@]}" diff "${DIFF_OPTS[@]}" --name-status -z "${base_ref}..${target_ref}")
    [[ ${#NEG_PATH_ARGS[@]} -gt 0 ]] && git_cmd_args+=("${NEG_PATH_ARGS[@]}")
    [[ ${#PATH_ARGS[@]} -gt 0 ]] && git_cmd_args+=(-- "${PATH_ARGS[@]}")
    
    while IFS= read -r -d '' status; do
        local p1 p2 file
        IFS= read -r -d '' p1 || true
        file="$p1"
        case "${status:0:1}" in
            R|C) IFS= read -r -d '' p2 || true; file="$p2" ;;
        esac

        case "${status:0:1}" in
            A)
                added=$((added + 1))
                case "$(classify_path "$file")" in
                    30) new_src=$((new_src + 1)) ;;
                    10) new_tst=$((new_tst + 1)) ;;
                    20) new_doc=$((new_doc + 1)) ;;
                esac
                ;;
            M|T|R|C) modified=$((modified + 1)) ;; # typechange/rename/copy as modification
            D)       deleted=$((deleted + 1)) ;;
            *)       modified=$((modified + 1)) ;; # treat unknown as modification
        esac
    done < <(git "${git_cmd_args[@]}" 2>/dev/null)

    # insertions/deletions
    local numstat_cmd_args=("${CFG[@]}" diff "${DIFF_OPTS[@]}" --numstat "${base_ref}..${target_ref}")
    [[ ${#NEG_PATH_ARGS[@]} -gt 0 ]] && numstat_cmd_args+=("${NEG_PATH_ARGS[@]}")
    [[ ${#PATH_ARGS[@]} -gt 0 ]] && numstat_cmd_args+=(-- "${PATH_ARGS[@]}")
    
    local numstat_output
    numstat_output=$(sum_numstat git "${numstat_cmd_args[@]}")
    IFS=$'\t' read -r ins dels <<< "$numstat_output"

    print_results "$added" "$modified" "$deleted" "$new_src" "$new_tst" "$new_doc" "$ins" "$dels"

    # optional exit code contract
    if [[ "$EXIT_CODE_ON_CHANGE" == true ]]; then
        return 1
    fi
    return 0
}

# --- Main --------------------------------------------------------------------
analyze_file_changes "$BASE_REF" "$TARGET_REF" 