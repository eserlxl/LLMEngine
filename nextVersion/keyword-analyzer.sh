#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Keyword analyzer for next-version
# Analyzes keywords in git diffs and commit messages

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

readonly PROG=${0##*/}

# ------------------------- error handling ------------------------------------
# shellcheck disable=SC2154
trap 'rc=$?; echo "Error: ${PROG}: line ${LINENO}: ${BASH_COMMAND}" >&2; exit $rc' ERR

# ------------------------- usage ---------------------------------------------
show_help() {
    cat << 'EOF'
Keyword Analyzer

Usage:
  $(basename "$0") --base <ref> [options]

Options:
  --base <ref>             Base reference for comparison (required)
  --target <ref>           Target reference for comparison (default: HEAD)
  --repo-root <path>       Set repository root directory
  --only-paths <globs>     Restrict analysis to comma-separated path globs
  --ignore-whitespace      Ignore whitespace changes in diff analysis
  --added-only             Count only added lines (excludes diff headers like '+++')
  --no-merges              Exclude merge commits when scanning messages
  --format <fmt>           Output format: human | kv | json  (default: human)
  --fail-on <what>         Exit non-zero on signal: none|any|break|security (default: none)
  --verbose                Verbose diagnostics
  --machine                (deprecated) same as --format kv
  --json                   (deprecated) same as --format json
  --help, -h               Show this help

Examples:
  $(basename "$0") --base v1.0.0 --target HEAD
  $(basename "$0") --base HEAD~5 --target HEAD --format kv
  $(basename "$0") --base v1.0.0 --target v1.1.0 --format json --fail-on break
EOF
}

# ------------------------- defaults ------------------------------------------
BASE_REF=""
TARGET_REF="HEAD"
REPO_ROOT=""
ONLY_PATHS=""
IGNORE_WHITESPACE=false
ADDED_ONLY=false
FORMAT="human"        # human|kv|json
NO_MERGES=false
VERBOSE=false
FAIL_ON="none"        # none|any|break|security

# ------------------------- helpers -------------------------------------------
# die() function is now sourced from version-utils.sh

# vnote() { $VERBOSE && printf '[%s] %s\n' "$PROG" "$*" || :; }  # Unused function - commented out to fix shellcheck warning

trim() {
    # trim leading/trailing whitespace (portable)
    local s="${1-}"
    s="${s#"${s%%[![:space:]]*}"}"
    s="${s%"${s##*[![:space:]]}"}"
    printf '%s' "$s"
}

ensure_git() {
    command -v git >/dev/null 2>&1 || die "git command not found"
}

ensure_repo() {
    git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "Not inside a git repository"
}

ensure_ref() {
    local ref="$1"
    git rev-parse --verify --quiet "${ref}^{commit}" >/dev/null || die "Invalid ref: ${ref}"
}

# int_or_zero() function is now replaced with is_uint from version-utils.sh
# int_or_zero() {
#     # prints only digits; returns 0 if empty
#     local v="${1:-0}"
#     v="$(printf '%s' "$v" | tr -cd '0-9')"
#     if [[ -z "$v" ]]; then v=0; fi
#     printf '%s' "$v"
# }

count_matches() {
    # stdin + regex -> integer (line-based count, case-insensitive)
    # usage: printf '%s' "$data" | count_matches 'PATTERN'
    local pattern="$1"
    local n
    n="$(grep -Eci -- "$pattern" 2>/dev/null || true)"
    is_uint "$n" && printf '%s' "$n" || printf '0'
}

json_bool() {
    # echo proper JSON booleans
    case "${1:-false}" in
        true|TRUE|1)  printf 'true' ;;
        *)            printf 'false' ;;
    esac
}

# ------------------------- parse args ----------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --base)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--base requires a value"
            BASE_REF="$2"; shift 2;;
        --target)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--target requires a value"
            TARGET_REF="$2"; shift 2;;
        --repo-root)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--repo-root requires a value"
            REPO_ROOT="$2"; shift 2;;
        --only-paths)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--only-paths requires a comma-separated list"
            ONLY_PATHS="$2"; shift 2;;
        --ignore-whitespace) IGNORE_WHITESPACE=true; shift;;
        --added-only)        ADDED_ONLY=true; shift;;
        --no-merges)         NO_MERGES=true; shift;;
        --format)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--format requires a value"
            case "$2" in
                human|kv|json) FORMAT="$2" ;;
                *) die "--format must be one of: human|kv|json" ;;
            esac
            shift 2;;
        --fail-on)
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--fail-on requires a value"
            case "$2" in
                none|any|break|security) FAIL_ON="$2" ;;
                *) die "--fail-on must be one of: none|any|break|security" ;;
            esac
            shift 2;;
        --verbose)           VERBOSE=true; shift;;
        --machine) FORMAT="kv"; shift;;
        --json)    FORMAT="json"; shift;;
        --help|-h) show_help; exit 0;;
        *) die "Unknown option: $1" ;;
    esac
done

[[ -n "$BASE_REF" ]] || die "--base is required"

ensure_git

if [[ -n "$REPO_ROOT" ]]; then
    cd "$REPO_ROOT" || die "Cannot cd to repo-root: $REPO_ROOT"
fi

ensure_repo
ensure_ref "$BASE_REF"
ensure_ref "$TARGET_REF"

# Build path args
PATH_ARGS=()
if [[ -n "$ONLY_PATHS" ]]; then
    IFS=',' read -r -a _paths <<< "$ONLY_PATHS"
    for p in "${_paths[@]}"; do
        p="$(trim "$p")"
        [[ -n "$p" ]] && PATH_ARGS+=("$p")
    done
    if [[ ${#PATH_ARGS[@]} -gt 0 ]]; then
        PATH_ARGS=(-- "${PATH_ARGS[@]}")
    fi
fi

$VERBOSE && { printf '[%s] base=%s target=%s\n' "$PROG" "$BASE_REF" "$TARGET_REF"; [[ ${#PATH_ARGS[@]} -gt 0 ]] && printf '[%s] paths=%s\n' "$PROG" "${PATH_ARGS[*]}"; }

# ------------------------- collect data --------------------------------------

# Prepare git diff
DIFF_OPTS=(-M -C --unified=0 --no-ext-diff)
if [[ "$IGNORE_WHITESPACE" == "true" ]]; then
    DIFF_OPTS+=(-w)
fi

# consistent filename quoting off (avoid \nnn escapes)
git_config=(-c color.ui=false -c core.quotepath=false)

# Get raw diff
DIFF_CONTENT="$(git "${git_config[@]}" diff "${DIFF_OPTS[@]}" "$BASE_REF..$TARGET_REF" "${PATH_ARGS[@]}" 2>/dev/null || true)"

# Optionally restrict to added lines only (strip headers)
if [[ "$ADDED_ONLY" == "true" ]]; then
    # Keep lines that start with '+' but not '+++'
    # shellcheck disable=SC2001
    DIFF_CONTENT="$(printf '%s\n' "$DIFF_CONTENT" | sed -n '/^+/p' | sed -n '/^\+\+\+/!p')"
fi

# Get commit messages (subject + body) in range
log_opts=(--format='%s %b')
$NO_MERGES && log_opts+=(--no-merges)
COMMIT_MESSAGES="$(git "${git_config[@]}" log "${log_opts[@]}" "$BASE_REF..$TARGET_REF" 2>/dev/null || true)"

# ------------------------- patterns ------------------------------------------
# Upgrade: scan keywords ANYWHERE in the diff (align with C++ analyzer)
# Keep tokens configurable
readonly TOKEN_CLI_BREAKING='CLI[- ]?BREAKING'
readonly TOKEN_API_BREAKING='API[- ]?BREAKING'
readonly TOKEN_NEW_FEATURE='NEW[- ]?FEATURE'
readonly TOKEN_SECURITY='SECURITY'
readonly TOKEN_REMOVED_OPT='REMOVED[[:space:]]+OPTION(S)?'
readonly TOKEN_ADDED_OPT='ADDED[[:space:]]+OPTION(S)?'

# For diff scanning, use plain tokens for BREAKING/feature/option keywords.
# Keep SECURITY restricted to comment contexts to align with C++ analyzer behavior.
PAT_CLI_BREAKING_CODE="$TOKEN_CLI_BREAKING"
PAT_API_BREAKING_CODE="$TOKEN_API_BREAKING"
PAT_NEW_FEATURE_CODE="$TOKEN_NEW_FEATURE"
PAT_SECURITY_CODE="$(printf '(^|[[:space:]])[+-]?[[:space:]]*(//|/\\*|#|--)\s*%s' "$TOKEN_SECURITY")"
PAT_REMOVED_OPT_CODE="$TOKEN_REMOVED_OPT"
PAT_ADDED_OPT_CODE="$TOKEN_ADDED_OPT"

# Commit message patterns (looser)
readonly PAT_CLI_BREAKING_COMMIT='(CLI[- ]?BREAKING|BREAKING[^[:alnum:]]+.*CLI)'
readonly PAT_API_BREAKING_COMMIT='(API[- ]?BREAKING|BREAKING[^[:alnum:]]+.*API)'
readonly PAT_GENERAL_BREAKING_COMMIT='(BREAKING[[:space:]]+CHANGE|BREAKING[^[:alnum:]]+.*(CHANGE|MAJOR))'
readonly PAT_NEW_FEATURE_COMMIT='(NEW[- ]?FEATURE|FEATURE(S)?[^[:alnum:]]+.*(ADD(ED)?|INTRODUC(ED|ES)))'
readonly PAT_SECURITY_COMMIT='(SECURITY|VULNERABILIT(Y|IES)|CVE[- ]?[0-9]{4}-[0-9]+)'

# ------------------------- counting ------------------------------------------
cli_breaking_keywords="$(printf '%s' "$DIFF_CONTENT" | count_matches "$PAT_CLI_BREAKING_CODE")"
api_breaking_keywords="$(printf '%s' "$DIFF_CONTENT" | count_matches "$PAT_API_BREAKING_CODE")"
new_feature_keywords="$(printf '%s' "$DIFF_CONTENT" | count_matches "$PAT_NEW_FEATURE_CODE")"
security_keywords="$(printf '%s' "$DIFF_CONTENT" | count_matches "$PAT_SECURITY_CODE")"
removed_options_keywords="$(printf '%s' "$DIFF_CONTENT" | count_matches "$PAT_REMOVED_OPT_CODE")"
added_options_keywords="$(printf '%s' "$DIFF_CONTENT" | count_matches "$PAT_ADDED_OPT_CODE")"

commit_cli_breaking="$(printf '%s' "$COMMIT_MESSAGES" | count_matches "$PAT_CLI_BREAKING_COMMIT")"
commit_api_breaking="$(printf '%s' "$COMMIT_MESSAGES" | count_matches "$PAT_API_BREAKING_COMMIT")"
commit_general_breaking="$(printf '%s' "$COMMIT_MESSAGES" | count_matches "$PAT_GENERAL_BREAKING_COMMIT")"
commit_new_feature="$(printf '%s' "$COMMIT_MESSAGES" | count_matches "$PAT_NEW_FEATURE_COMMIT")"
commit_security="$(printf '%s' "$COMMIT_MESSAGES" | count_matches "$PAT_SECURITY_COMMIT")"

total_cli_breaking=$(( cli_breaking_keywords + commit_cli_breaking ))
total_api_breaking=$(( api_breaking_keywords + commit_api_breaking ))
total_general_breaking=$(( commit_general_breaking ))
total_new_features=$(( new_feature_keywords + commit_new_feature ))
total_security=$(( security_keywords + commit_security ))

has_cli_breaking=false;   [[ $total_cli_breaking   -gt 0 ]] && has_cli_breaking=true
has_api_breaking=false;   [[ $total_api_breaking   -gt 0 ]] && has_api_breaking=true
has_general_breaking=false; [[ $total_general_breaking -gt 0 ]] && has_general_breaking=true
has_new_features=false;   [[ $total_new_features   -gt 0 ]] && has_new_features=true
has_security=false;       [[ $total_security       -gt 0 ]] && has_security=true
has_removed_options=false;[[ $removed_options_keywords -gt 0 ]] && has_removed_options=true
has_added_options=false;  [[ $added_options_keywords   -gt 0 ]] && has_added_options=true

# ------------------------- output --------------------------------------------
case "$FORMAT" in
  json)
    printf '{\n'
    printf '  "cli_breaking_keywords": %s,\n' "$cli_breaking_keywords"
    printf '  "api_breaking_keywords": %s,\n' "$api_breaking_keywords"
    printf '  "commit_cli_breaking": %s,\n' "$commit_cli_breaking"
    printf '  "commit_api_breaking": %s,\n' "$commit_api_breaking"
    printf '  "commit_general_breaking": %s,\n' "$commit_general_breaking"
    printf '  "total_cli_breaking": %s,\n' "$total_cli_breaking"
    printf '  "total_api_breaking": %s,\n' "$total_api_breaking"
    printf '  "total_general_breaking": %s,\n' "$total_general_breaking"
    printf '  "new_feature_keywords": %s,\n' "$new_feature_keywords"
    printf '  "commit_new_feature": %s,\n' "$commit_new_feature"
    printf '  "total_new_features": %s,\n' "$total_new_features"
    printf '  "security_keywords": %s,\n' "$security_keywords"
    printf '  "commit_security": %s,\n' "$commit_security"
    printf '  "total_security": %s,\n' "$total_security"
    printf '  "removed_options_keywords": %s,\n' "$removed_options_keywords"
    printf '  "added_options_keywords": %s,\n' "$added_options_keywords"
    printf '  "has_cli_breaking": %s,\n' "$(json_bool "$has_cli_breaking")"
    printf '  "has_api_breaking": %s,\n' "$(json_bool "$has_api_breaking")"
    printf '  "has_general_breaking": %s,\n' "$(json_bool "$has_general_breaking")"
    printf '  "has_new_features": %s,\n' "$(json_bool "$has_new_features")"
    printf '  "has_security": %s,\n' "$(json_bool "$has_security")"
    printf '  "has_removed_options": %s,\n' "$(json_bool "$has_removed_options")"
    printf '  "has_added_options": %s\n' "$(json_bool "$has_added_options")"
    printf '}\n'
    ;;
  kv)
    printf 'CLI_BREAKING_KEYWORDS=%s\n' "$cli_breaking_keywords"
    printf 'API_BREAKING_KEYWORDS=%s\n' "$api_breaking_keywords"
    printf 'COMMIT_CLI_BREAKING=%s\n' "$commit_cli_breaking"
    printf 'COMMIT_API_BREAKING=%s\n' "$commit_api_breaking"
    printf 'COMMIT_GENERAL_BREAKING=%s\n' "$commit_general_breaking"
    printf 'TOTAL_CLI_BREAKING=%s\n' "$total_cli_breaking"
    printf 'TOTAL_API_BREAKING=%s\n' "$total_api_breaking"
    printf 'TOTAL_GENERAL_BREAKING=%s\n' "$total_general_breaking"
    printf 'NEW_FEATURE_KEYWORDS=%s\n' "$new_feature_keywords"
    printf 'COMMIT_NEW_FEATURE=%s\n' "$commit_new_feature"
    printf 'TOTAL_NEW_FEATURES=%s\n' "$total_new_features"
    printf 'SECURITY_KEYWORDS=%s\n' "$security_keywords"
    printf 'COMMIT_SECURITY=%s\n' "$commit_security"
    printf 'TOTAL_SECURITY=%s\n' "$total_security"
    printf 'REMOVED_OPTIONS_KEYWORDS=%s\n' "$removed_options_keywords"
    printf 'ADDED_OPTIONS_KEYWORDS=%s\n' "$added_options_keywords"
    printf 'HAS_CLI_BREAKING=%s\n' "$has_cli_breaking"
    printf 'HAS_API_BREAKING=%s\n' "$has_api_breaking"
    printf 'HAS_GENERAL_BREAKING=%s\n' "$has_general_breaking"
    printf 'HAS_NEW_FEATURES=%s\n' "$has_new_features"
    printf 'HAS_SECURITY=%s\n' "$has_security"
    printf 'HAS_REMOVED_OPTIONS=%s\n' "$has_removed_options"
    printf 'HAS_ADDED_OPTIONS=%s\n' "$has_added_options"
    ;;
  human)
    printf '=== Keyword Analysis (%s..%s) ===\n' "$BASE_REF" "$TARGET_REF"
    [[ -n "$ONLY_PATHS" ]] && printf 'Paths: %s\n' "$ONLY_PATHS"
    [[ "$IGNORE_WHITESPACE" == "true" ]] && printf '(ignoring whitespace)\n'
    [[ "$ADDED_ONLY" == "true" ]] && printf '(added lines only)\n'
    [[ "$NO_MERGES" == "true" ]] && printf '(excluding merge commits)\n'
    printf '\nBreaking Change Keywords:\n'
    printf '  CLI-BREAKING in code:    %s\n' "$cli_breaking_keywords"
    printf '  API-BREAKING in code:    %s\n' "$api_breaking_keywords"
    printf '  CLI-BREAKING in commits: %s\n' "$commit_cli_breaking"
    printf '  API-BREAKING in commits: %s\n' "$commit_api_breaking"
    printf '  Total CLI breaking:      %s\n' "$total_cli_breaking"
    printf '  Total API breaking:      %s\n' "$total_api_breaking"
    printf '\nFeature Keywords:\n'
    printf '  NEW-FEATURE in code:     %s\n' "$new_feature_keywords"
    printf '  NEW-FEATURE in commits:  %s\n' "$commit_new_feature"
    printf '  Total new features:      %s\n' "$total_new_features"
    printf '\nSecurity Keywords:\n'
    printf '  SECURITY in code:        %s\n' "$security_keywords"
    printf '  SECURITY in commits:     %s\n' "$commit_security"
    printf '  Total security:          %s\n' "$total_security"
    printf '\nOption Keywords:\n'
    printf '  Removed options:         %s\n' "$removed_options_keywords"
    printf '  Added options:           %s\n' "$added_options_keywords"
    printf '\nSummary:\n'
    printf '  Has CLI breaking:        %s\n' "$has_cli_breaking"
    printf '  Has API breaking:        %s\n' "$has_api_breaking"
    printf '  Has new features:        %s\n' "$has_new_features"
    printf '  Has security:            %s\n' "$has_security"
    printf '  Has removed options:     %s\n' "$has_removed_options"
    printf '  Has added options:       %s\n' "$has_added_options"
    ;;
esac

# ------------------------- CI gating -----------------------------------------
exit_rc=0
case "$FAIL_ON" in
  any)      $has_cli_breaking || $has_api_breaking || $has_security || $has_new_features || $has_removed_options || $has_added_options && exit_rc=2 ;;
  break)    $has_cli_breaking || $has_api_breaking && exit_rc=3 ;;
  security) $has_security && exit_rc=4 ;;
  none)     : ;;
esac
exit "$exit_rc" 