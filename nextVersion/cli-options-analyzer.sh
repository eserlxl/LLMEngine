#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# CLI options analyzer for next-version
# Analyzes CLI/API breaking changes in git diffs

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C

# Source common utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# --- utils -------------------------------------------------------------------

# die() function is now sourced from version-utils.sh
note() { :; } # overwritten by --verbose

boolstr() { [[ "$1" == true ]] && printf 'true' || printf 'false'; }

# json_escape() function is now sourced from version-utils.sh

trim_spaces() {
  # trim leading/trailing ASCII and Unicode spaces
  printf '%s' "$1" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//'
}

one_line() {  # turn a newline-separated list into space-separated one line
  paste -sd' ' -
}

join_by() { local IFS="$1"; shift; printf '%s' "$*"; }

# require_cmd() function is now sourced from version-utils.sh

have_git() { command -v git >/dev/null 2>&1; }

verify_git_repo() {
  git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "Not inside a git repository"
}

# verify_ref() function is now sourced from version-utils.sh

# comm requires sorted input
set_diff_counts() {
  # args: before_list after_list; outputs: REM= count, ADD= count to named vars
  local before="$1" after="$2" _rem _add
  # Handle multi-line input correctly by ensuring each item is on a separate line
  _rem=$(comm -23 <(printf '%s\n' "$before" | LC_ALL=C sort -u) <(printf '%s\n' "$after" | LC_ALL=C sort -u) | wc -l | tr -d ' ')
  _add=$(comm -13 <(printf '%s\n' "$before" | LC_ALL=C sort -u) <(printf '%s\n' "$after" | LC_ALL=C sort -u) | wc -l | tr -d ' ')
  printf '%s %s\n' "$_rem" "$_add"
}

# --- help --------------------------------------------------------------------

show_help() {
    cat << EOF
CLI Options Analyzer

Usage: $(basename "$0") [options]

Options:
  --base <ref>             Base reference for comparison (required)
  --target <ref>           Target reference for comparison (default: HEAD)
  --repo-root <path>       Set repository root directory
  --only-paths <globs>     Comma-separated glob pathspecs (git-style). Example:
                           ":(glob)src/**,:(glob)include/**"
  --ignore-whitespace      Ignore whitespace changes in diff analysis
  --machine                Output machine-readable key=value format
  --json                   Output machine-readable JSON
  --fail-on-breaking       Exit with code 2 if breaking CLI/API detected
  --verbose                Print progress to stderr
  --help, -h               Show this help

Examples:
  $(basename "$0") --base v1.0.0 --target HEAD
  $(basename "$0") --base HEAD~5 --target HEAD --machine
  $(basename "$0") --base v1.0.0 --target v1.1.0 --json
EOF
}

# --- args --------------------------------------------------------------------

BASE_REF=""
TARGET_REF="HEAD"
REPO_ROOT=""
ONLY_PATHS=""
IGNORE_WHITESPACE=false
MACHINE_OUTPUT=false
JSON_OUTPUT=false
FAIL_ON_BREAKING=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
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
            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--only-paths requires a comma-separated globs list"
            ONLY_PATHS="$2"; shift 2;;
        --ignore-whitespace) IGNORE_WHITESPACE=true; shift;;
        --machine) MACHINE_OUTPUT=true; shift;;
        --json) JSON_OUTPUT=true; shift;;
        --fail-on-breaking) FAIL_ON_BREAKING=true; shift;;
        --verbose) VERBOSE=true; shift;;
        --help|-h) show_help; exit 0;;
        *) show_help; die "Unknown option: $1";;
    esac
done

$VERBOSE && note() { printf '[cli-analyzer] %s\n' "$*" >&2; }

[[ -n "$BASE_REF" ]] || die "--base is required"
# Default baseline tools are checked inside require_cmd() when no args are passed.
# shellcheck disable=SC2119
require_cmd
have_git || die "git command not found"

# Change to repo root if specified
if [[ -n "$REPO_ROOT" ]]; then
  cd "$REPO_ROOT" || die "Cannot cd into $REPO_ROOT"
fi
verify_git_repo
verify_ref "$BASE_REF"
verify_ref "$TARGET_REF"

# --- pathspecs ---------------------------------------------------------------

declare -a PATHSPEC
if [[ -n "$ONLY_PATHS" ]]; then
  IFS=',' read -r -a _paths <<< "$ONLY_PATHS"
  for p in "${!_paths[@]}"; do
    _paths[p]=$(trim_spaces "${_paths[$p]}")
  done
  # pass through verbatim; caller can use :(glob) or plain
  for p in "${_paths[@]}"; do [[ -n "$p" ]] && PATHSPEC+=("$p"); done
else
  # default: common recursive C/C++ sources and headers
  PATHSPEC+=(":(glob)**/*.c" ":(glob)**/*.cc" ":(glob)**/*.cpp" ":(glob)**/*.cxx")
  PATHSPEC+=(":(glob)**/*.h" ":(glob)**/*.hh" ":(glob)**/*.hpp")
fi

# --- diff flags --------------------------------------------------------------

DIFF_FLAGS=(-M -C --unified=0)
$IGNORE_WHITESPACE && DIFF_FLAGS+=(-w)



# --- extraction functions ----------------------------------------------------

# Parse short options from getopt()/getopt_long() call sites.
extract_short_opts() {
  # Concatenate optstrings found in quotes in getopt calls, then uniq.
  grep -Eaa -- 'getopt(_long)?[[:space:]]*\(' \
  | grep -Eao -- '"[^"]*"' \
  | tr -d '"' \
  | LC_ALL=C sort -u \
  | tr -d '\n'
}

# Parse long options from struct option arrays:
#  - classic initializer: { "name", has_arg, flag, val }
#  - designated initializer: { .name = "name", ... }
extract_long_opts() {
  awk '
    /struct[[:space:]]+option/ { inblk=1 }
    inblk && /};/             { inblk=0 }
    inblk {
      # capture first quoted token inside braces
      if (match($0, /{\s*"([^"]+)"/, a)) print a[1];
      # capture designated initializer .name = "xxx"
      if (match($0, /\.name[[:space:]]*=[[:space:]]*"([^"]+)"/, b)) print b[1];
    }
  ' \
  | sed '/^$/d' \
  | LC_ALL=C sort -u \
  | paste -sd',' -
}

# Count removed prototypes in headers as API break indicator.
count_removed_prototypes() {
  awk '
    # minus lines only (handle line numbers from grep -n)
    /^[0-9]+:-/ || /^-/ {
      # crude prototype detector: ret name(args);
      if ($0 !~ /^[0-9]*:-[[:space:]]*(typedef|#)/ && $0 !~ /^-[[:space:]]*(typedef|#)/ &&
          ($0 ~ /^[0-9]*:-[[:space:]]*[A-Za-z_][A-Za-z0-9_[:space:]\*]+[[:space:]]+[A-Za-z_][A-Za-z0-9_]*\([^;]*\)[[:space:]]*;[[:space:]]*$/ ||
           $0 ~ /^-[[:space:]]*[A-Za-z_][A-Za-z0-9_[:space:]\*]+[[:space:]]+[A-Za-z_][A-Za-z0-9_]*\([^;]*\)[[:space:]]*;[[:space:]]*$/))
        print
    }
  ' | wc -l | tr -d ' '
}

# Manual diff-based long option detection on added/removed lines (avoid strings/comments roughly).
scan_manual_long_opts() {
  local sign="$1" # '+' or '-'
  awk -v s="$sign" '
    $0 ~ ("^" s) {
      line=$0
      # Skip diff headers and hunk lines to avoid false positives like "--- a/file"
      if (line ~ /^(\+\+\+|---|@@)/) next
      # Skip obvious comments and quoted strings (naive)
      if (line ~ /(^[+-]\s*\/\/)|(^[+-]\s*\/\*)|(^[+-].*["'\''].*--)/) next
      # Require at least one alphanumeric after -- to avoid matching sequences like "---"
      # Handle both unified=0 format (entire line) and unified=1+ format (individual lines)
      if (match(line, /--([A-Za-z0-9][A-Za-z0-9-]*)/, m)) print "--" m[1]
    }
  ' | LC_ALL=C sort -u
}

# Enhanced manual CLI detection that looks for CLI parsing patterns beyond just long options
scan_manual_cli_patterns() {
  local sign="$1" # '+' or '-'
  awk -v s="$sign" '
    $0 ~ ("^" s) {
      line=$0
      # Skip diff headers and hunk lines
      if (line ~ /^(\+\+\+|---|@@)/) next
      # Skip obvious comments
      if (line ~ /(^[+-]\s*\/\/)|(^[+-]\s*\/\*)/) next
      
      # Look for CLI parsing patterns
      # 1. Long options in string comparisons (e.g., strcmp(argv[i], "--help"))
      if (match(line, /["'\'']--([A-Za-z0-9][A-Za-z0-9-]*)/, m)) {
        print "--" m[1]
        next
      }
      
      # 2. Long options in if statements (e.g., if (strcmp(argv[i], "--help") == 0))
      if (match(line, /--([A-Za-z0-9][A-Za-z0-9-]*)/, m)) {
        print "--" m[1]
        next
      }
      
      # 3. CLI parsing logic (e.g., argc, argv usage)
      if (line ~ /argc|argv/) {
        # Look for long options in the same line
        if (match(line, /--([A-Za-z0-9][A-Za-z0-9-]*)/, m)) {
          print "--" m[1]
        }
      }
    }
  ' | LC_ALL=C sort -u
}

# Switch-case removed vs added (roughly ignore comments)
scan_case_labels() {
  local sign="$1" # '+' or '-'
  awk -v s="$sign" '
    $0 ~ ("^" s) {
      line=$0
      if (line ~ /(^[+-]\s*\/\/)|(^[+-]\s*\/\*)/) next
      if (match(line, /case[[:space:]]+([^:[:space:]]+)[[:space:]]*:/, m)) print m[1]
    }
  ' | LC_ALL=C sort -u
}

# --- output functions --------------------------------------------------------

emit_results_zero() {
  note "No relevant source/header changes"
  if $JSON_OUTPUT; then
    cat <<'JSON'
{
  "cli_changes": false,
  "breaking_cli_changes": false,
  "api_breaking": false,
  "manual_cli_changes": false,
  "manual_added_long_count": 0,
  "manual_removed_long_count": 0,
  "removed_short_count": 0,
  "added_short_count": 0,
  "removed_long_count": 0,
  "added_long_count": 0,
  "getopt_changes": 0,
  "arg_parsing_changes": 0,
  "help_text_changes": 0,
  "main_signature_changes": 0,
  "enhanced_cli_patterns": 0,
  "removed_short_list": [],
  "added_short_list": [],
  "removed_long_list": [],
  "added_long_list": []
}
JSON
  elif $MACHINE_OUTPUT; then
    cat <<EOF
CLI_CHANGES=false
BREAKING_CLI_CHANGES=false
API_BREAKING=false
MANUAL_CLI_CHANGES=false
MANUAL_ADDED_LONG_COUNT=0
MANUAL_REMOVED_LONG_COUNT=0
REMOVED_SHORT_COUNT=0
ADDED_SHORT_COUNT=0
REMOVED_LONG_COUNT=0
ADDED_LONG_COUNT=0
GETOPT_CHANGES=0
ARG_PARSING_CHANGES=0
HELP_TEXT_CHANGES=0
MAIN_SIGNATURE_CHANGES=0
ENHANCED_CLI_PATTERNS=0
EOF
  else
    printf '=== CLI Options Analysis ===\nNo relevant source/header changes between %s..%s\n' "$BASE_REF" "$TARGET_REF"
  fi
}

# --- gather changes ----------------------------------------------------------

note "Collecting changed files…"
# Use -z to be robust to spaces/newlines; include renames/copies.
mapfile -d '' CHANGED_FILES < <(git -c color.ui=false -c core.quotepath=false diff -z -M -C --name-only "$BASE_REF".."$TARGET_REF" -- "${PATHSPEC[@]}" || true)

if (( ${#CHANGED_FILES[@]} == 0 )); then
  emit_results_zero
  exit 0
fi

# Pre-fetch contents (avoid repeated git show)
declare -A BEFORE AFTER
for f in "${CHANGED_FILES[@]}"; do
  BEFORE["$f"]=$(git -c color.ui=false show "$BASE_REF:$f" 2>/dev/null || true)
  AFTER["$f"]=$(git -c color.ui=false show "$TARGET_REF:$f" 2>/dev/null || true)
done

# Combined diffs (once)
note "Computing diffs…"
SRC_DIFF=$(git -c color.ui=false diff "${DIFF_FLAGS[@]}" "$BASE_REF".."$TARGET_REF" -- "${CHANGED_FILES[@]}" || true)
CPP_DIFF=$(git -c color.ui=false diff "${DIFF_FLAGS[@]}" "$BASE_REF".."$TARGET_REF" -- "${PATHSPEC[@]}" || true)

# Header-only diff subset (kept for reference), but API prototype removals should be checked across the entire diff
# Kept for reference/debug: header-only diff subset
# shellcheck disable=SC2034
HDR_DIFF=$(printf '%s' "$SRC_DIFF" | grep -E '^\+|^-|^diff --git|^index|^@@|/.*\.(h|hh|hpp)$' -n --color=never | sed -n 'p' || true)

# --- extract option sets -----------------------------------------------------

short_before_all="" ; short_after_all=""
long_before_all=""  ; long_after_all=""

for f in "${CHANGED_FILES[@]}"; do
  # Extract optstrings from getopt()/getopt_long() calls: "abc:d:"
  short_before_all+=$(printf '%s' "${BEFORE[$f]}" | extract_short_opts || true)
  short_after_all+=$(printf '%s' "${AFTER[$f]}"  | extract_short_opts || true)

  # Extract long options from struct option arrays: { "name", …
  long_before_all+=$(printf '%s' "${BEFORE[$f]}" | extract_long_opts || true)
  long_after_all+=$(printf '%s' "${AFTER[$f]}"  | extract_long_opts || true)
done

short_before=$(printf '%s' "$short_before_all" | LC_ALL=C sort -u | tr -d '\n')
short_after=$( printf '%s' "$short_after_all"  | LC_ALL=C sort -u | tr -d '\n')

long_before=$( printf '%s' "$long_before_all" | tr ',' '\n' | sed '/^$/d' | LC_ALL=C sort -u | tr '\n' ',' | sed 's/,$//')
long_after=$(  printf '%s' "$long_after_all"  | tr ',' '\n' | sed '/^$/d' | LC_ALL=C sort -u | tr '\n' ',' | sed 's/,$//')

# Build lists for diff
short_before_list=$(printf '%s' "$short_before" | tr -d ':' | fold -w1 | sed '/^$/d' | LC_ALL=C sort -u)
short_after_list=$( printf '%s' "$short_after"  | tr -d ':' | fold -w1 | sed '/^$/d' | LC_ALL=C sort -u)
long_before_list=$(  printf '%s' "$long_before" | tr ',' '\n' | sed '/^$/d' | LC_ALL=C sort -u)
long_after_list=$(   printf '%s' "$long_after"  | tr ',' '\n' | sed '/^$/d' | LC_ALL=C sort -u)

# Compute removed/added lists
removed_short_list=$(comm -23 <(printf '%s\n' "$short_before_list") <(printf '%s\n' "$short_after_list") || true)
added_short_list=$(  comm -13 <(printf '%s\n' "$short_before_list") <(printf '%s\n' "$short_after_list") || true)
removed_long_list=$( comm -23 <(printf '%s\n' "$long_before_list")  <(printf '%s\n' "$long_after_list")  || true)
added_long_list=$(   comm -13 <(printf '%s\n' "$long_before_list")  <(printf '%s\n' "$long_after_list")  || true)

# Counts
read -r removed_short_count added_short_count < <(set_diff_counts "$short_before_list" "$short_after_list")
removed_short_count=$(printf '%s' "$removed_short_count" | tr -d ' ')
added_short_count=$(printf '%s' "$added_short_count" | tr -d ' ')
read -r removed_long_count  added_long_count  < <(set_diff_counts "$long_before_list"  "$long_after_list")
removed_long_count=$(printf '%s' "$removed_long_count" | tr -d ' ')
added_long_count=$(printf '%s' "$added_long_count" | tr -d ' ')

# Initialize pattern counters to avoid unbound variable errors under 'set -u'
getopt_changes=${getopt_changes:-0}
arg_parsing_changes=${arg_parsing_changes:-0}
help_text_changes=${help_text_changes:-0}
main_signature_changes=${main_signature_changes:-0}
enhanced_cli_patterns=${enhanced_cli_patterns:-0}

# Initialize CLI change flags
cli_changes=false
breaking_cli_changes=false
api_breaking=false
manual_cli_changes=false

# Normalize numeric counters to plain integers (strip leading zeros/empties)
removed_short_count=$(( ${removed_short_count:-0} + 0 ))
added_short_count=$(( ${added_short_count:-0} + 0 ))
removed_long_count=$(( ${removed_long_count:-0} + 0 ))
added_long_count=$(( ${added_long_count:-0} + 0 ))

# --- raw diff heuristics (ported from C++ logic) ------------------------------

# Port C++ regex contexts to bash
long_opt_re='--[A-Za-z0-9][A-Za-z0-9\-]*'
proto_removed_re='^-[^+].*[A-Za-z_][A-Za-z0-9_\s\*]+\s+[A-Za-z_][A-Za-z0-9_]*\([^;]*\)\s*;\s*$'
# short_opt_removed_re='^-[^+].*[^-]-[A-Za-z](\s|$)'  # Currently unused, kept for future use
case_label_re='case[[:space:]]+([^:[:space:]]+)[[:space:]]*:'
getopt_call_re='(getopt_long|getopt)[[:space:]]*\('
argc_argv_added_re='(^|[^[:alnum:]_])(argc|argv)([^[:alnum:]_]|$)'
help_usage_added_re='(^|[^[:alnum:]_])(usage|help|option|argument)([^[:alnum:]_]|$)'
short_option_added_re='^\+[^/#!].*-[A-Za-z](\s|$)'
long_option_added_re='^\+[^/#!].*--[A-Za-z0-9\-]+'
argc_check_added_re='^\+.*argc[[:space:]]*[<>=!]'
argv_access_added_re='^\+.*argv\['
# Exclude generic main() additions from enhanced CLI patterns to reduce noise
# Use a never-match pattern for bash ERE to effectively disable this check
main_signature_added_re='^$'

is_comment_line() {
  local ln="$1"
  [[ "$ln" =~ ^[+-][[:space:]]*(//|/\*) ]]
}

has_quoted_long_opt() {
  local ln="$1"
  [[ "$ln" == *"\""* && "$ln" == *"--"* ]]
}

# Process all C/C++ files for manual CLI detection, regardless of getopt/struct option changes
# This ensures that manual CLI changes are detected even when there are no getopt/struct option changes
removed_cases=$(printf '%s' "$SRC_DIFF" | grep -Ea -- "^-" | grep -Eao -- "$case_label_re" | awk '{print $2}' | sed 's/://g' | LC_ALL=C sort -u || true)
added_cases=$(printf '%s' "$SRC_DIFF" | grep -Ea -- "^\+" | grep -Eao -- "$case_label_re" | awk '{print $2}' | sed 's/://g' | LC_ALL=C sort -u || true)
missing_cases=$(comm -23 <(printf '%s\n' "$removed_cases" | sed '/^$/d') <(printf '%s\n' "$added_cases" | sed '/^$/d') || true)
breaking_cli_changes=false
[[ -n "$missing_cases" ]] && breaking_cli_changes=true

# Count removals on '-' lines from full diff
removed_struct_long=""
added_struct_long=""
# Initialize manual long option accumulators to avoid unbound variable errors under 'set -u'
removed_long_opts=""
added_long_opts=""
while IFS= read -r ln; do
  [[ "$ln" =~ ^- ]] || continue
  # struct-based long options removed
  while read -r tok; do
    [[ -n "$tok" ]] && removed_struct_long=$(printf '%s\n%s' "$removed_struct_long" "$tok")
  done < <(printf '%s' "$ln" | grep -Eao -- "$long_opt_re" || true)
  # prototype removal => API breaking (only for actual function prototypes, not CLI options)
  if [[ "$ln" =~ $proto_removed_re ]] && [[ ! "$ln" =~ $long_opt_re ]]; then
    api_breaking=true
  fi
  # short option removals - disabled to avoid conflicts with structured approach
  # The structured approach above correctly handles short option counting
  # if [[ "$ln" =~ $short_opt_removed_re ]]; then
  #   removed_short_count=$(( ${removed_short_count:-0} + 1 ))
  # fi
  # manual long option removals (skip comment lines only; allow quoted strings to detect manual CLI)
  if ! is_comment_line "$ln"; then
    while read -r tok; do
      [[ -n "$tok" ]] && removed_long_opts=$(printf '%s\n%s' "$removed_long_opts" "$tok")
    done < <(printf '%s' "$ln" | grep -Eao -- "$long_opt_re" || true)
  fi
done < <(printf '%s' "$CPP_DIFF" | sed -n '/^-/p')

# Count additions on '+' lines from full diff
while IFS= read -r ln; do
  [[ "$ln" =~ ^\+ ]] || continue
  if [[ "${ln,,}" =~ $help_usage_added_re ]]; then
    help_text_changes=$(( ${help_text_changes:-0} + 1 ))
  fi
  if [[ "$ln" =~ $getopt_call_re ]] || [[ "${ln,,}" =~ $argc_argv_added_re ]] || [[ "$ln" =~ $short_option_added_re ]] || [[ "$ln" =~ $long_option_added_re ]] || [[ "$ln" =~ $argc_check_added_re ]] || [[ "$ln" =~ $argv_access_added_re ]] || [[ "$ln" =~ $main_signature_added_re ]]; then
    enhanced_cli_patterns=$(( ${enhanced_cli_patterns:-0} + 1 ))
  fi
  # struct long option adds
  while read -r tok; do
    [[ -n "$tok" ]] && added_struct_long=$(printf '%s\n%s' "$added_struct_long" "$tok")
  done < <(printf '%s' "$ln" | grep -Eao -- "$long_opt_re" || true)
  # manual long option adds (skip comment lines only; allow quoted strings to detect manual CLI)
  if ! is_comment_line "$ln"; then
    while read -r tok; do
      [[ -n "$tok" ]] && added_long_opts=$(printf '%s\n%s' "$added_long_opts" "$tok")
    done < <(printf '%s' "$ln" | grep -Eao -- "$long_opt_re" || true)
  fi
done < <(printf '%s' "$SRC_DIFF" | sed -n '/^\+/p')

# Override struct-based long option add/remove counts to mirror C++
removed_long_count=$(printf '%s\n' "$removed_struct_long" | sed '/^$/d' | LC_ALL=C sort -u | wc -l | tr -d ' ' || printf '0')
added_long_count=$(printf '%s\n' "$added_struct_long" | sed '/^$/d' | LC_ALL=C sort -u | wc -l | tr -d ' ' || printf '0')

# API change hint across entire diff (fallback)
removed_prototypes=$(printf '%s' "$SRC_DIFF" | count_removed_prototypes || printf '0')
api_breaking=${api_breaking:-false}
(( removed_prototypes > 0 )) && api_breaking=true

$VERBOSE && note "Raw diff heuristics completed, moving to enhanced manual CLI detection"

# Enhanced manual CLI detection for ALL C/C++ files (not just those with getopt/struct options)
# This ensures that manual CLI changes are detected even when there are no getopt/struct option changes
# Use both the original scan_manual_long_opts and the enhanced scan_manual_cli_patterns
# Also analyze file contents directly for new files that might not show up in diffs
$VERBOSE && note "Starting enhanced manual CLI detection"
added_long_opts_orig=${added_long_opts:-$(printf '%s' "$CPP_DIFF" | scan_manual_long_opts '+')}
removed_long_opts_orig=${removed_long_opts:-$(printf '%s' "$CPP_DIFF" | scan_manual_long_opts '-')}
added_long_opts_enhanced=${added_long_opts:-$(printf '%s' "$CPP_DIFF" | scan_manual_cli_patterns '+')}
removed_long_opts_enhanced=${removed_long_opts:-$(printf '%s' "$CPP_DIFF" | scan_manual_cli_patterns '-')}
$VERBOSE && note "Diff-based detection: added_orig=$added_long_opts_orig, added_enhanced=$added_long_opts_enhanced"

# Analyze file contents directly for CLI patterns (especially important for new files)
added_long_opts_contents=""
removed_long_opts_contents=""
$VERBOSE && note "Starting file content analysis for ${#CHANGED_FILES[@]} files"
for f in "${CHANGED_FILES[@]}"; do
  # For new files or files with CLI changes, analyze the content directly
  if [[ -n "${AFTER[$f]}" ]]; then
    # Look for CLI patterns in the new/current content
    while IFS= read -r line; do
      # Look for long options in string comparisons (e.g., strcmp(argv[i], "--help"))
      if [[ "$line" =~ [\"'"'"'"]--([A-Za-z0-9][A-Za-z0-9-]*) ]]; then
        added_long_opts_contents=$(printf '%s\n%s' "$added_long_opts_contents" "--${BASH_REMATCH[1]}")
        $VERBOSE && note "Found CLI option in content: --${BASH_REMATCH[1]}"
      fi
      # Look for long options in if statements (e.g., if (strcmp(argv[i], "--help") == 0))
      if [[ "$line" =~ --([A-Za-z0-9][A-Za-z0-9-]*) ]]; then
        added_long_opts_contents=$(printf '%s\n%s' "$added_long_opts_contents" "--${BASH_REMATCH[1]}")
        $VERBOSE && note "Found CLI option in content: --${BASH_REMATCH[1]}"
      fi
      # Look for CLI parsing logic (e.g., argc, argv usage)
      if [[ "$line" =~ argc|argv ]]; then
        if [[ "$line" =~ --([A-Za-z0-9][A-Za-z0-9-]*) ]]; then
          added_long_opts_contents=$(printf '%s\n%s' "$added_long_opts_contents" "--${BASH_REMATCH[1]}")
          $VERBOSE && note "Found CLI option in content: --${BASH_REMATCH[1]}"
        fi
      fi
    done < <(printf '%s' "${AFTER[$f]}")
  fi
  
  # For removed files or files with CLI changes, analyze the old content
  if [[ -n "${BEFORE[$f]}" ]]; then
    # Look for CLI patterns in the old content
    while IFS= read -r line; do
      # Look for long options in string comparisons
      if [[ "$line" =~ [\"'"'"'"]--([A-Za-z0-9][A-Za-z0-9-]*) ]]; then
        removed_long_opts_contents=$(printf '%s\n%s' "$removed_long_opts_contents" "--${BASH_REMATCH[1]}")
      fi
      # Look for long options in if statements
      if [[ "$line" =~ --([A-Za-z0-9][A-Za-z0-9-]*) ]]; then
        removed_long_opts_contents=$(printf '%s\n%s' "$removed_long_opts_contents" "--${BASH_REMATCH[1]}")
      fi
      # Look for CLI parsing logic
      if [[ "$line" =~ argc|argv ]]; then
        if [[ "$line" =~ --([A-Za-z0-9][A-Za-z0-9-]*) ]]; then
          removed_long_opts_contents=$(printf '%s\n%s' "$removed_long_opts_contents" "--${BASH_REMATCH[1]}")
        fi
      fi
    done < <(printf '%s' "${BEFORE[$f]}")
  fi
done

# Combine all detection methods
added_long_opts=$(printf '%s\n%s\n%s' "$added_long_opts_orig" "$added_long_opts_enhanced" "$added_long_opts_contents" | sed '/^$/d' | LC_ALL=C sort -u)
removed_long_opts=$(printf '%s\n%s\n%s' "$removed_long_opts_orig" "$removed_long_opts_enhanced" "$removed_long_opts_contents" | sed '/^$/d' | LC_ALL=C sort -u)

manual_added_long_count=$(printf '%s\n' "$added_long_opts" | sed '/^$/d' | LC_ALL=C sort -u | wc -l | tr -d ' ' || printf '0')
manual_removed_long_count=$(printf '%s\n' "$removed_long_opts" | sed '/^$/d' | LC_ALL=C sort -u | wc -l | tr -d ' ' || printf '0')
manual_cli_changes=false
# Enhanced detection: any manual CLI pattern changes drive manual_cli_changes
(( manual_added_long_count > 0 || manual_removed_long_count > 0 )) && manual_cli_changes=true

# Always process manual CLI changes, even when there are no getopt/struct option changes
# This ensures that manual CLI detection works independently
if $manual_cli_changes; then
  cli_changes=true
fi

# Composite CLI change flags
# Initialize cli_changes based on manual detection, then check other conditions
if [[ -n "$short_after" && "$short_after" != "$short_before" ]]; then
  cli_changes=true
  [[ -n "$removed_short_list" ]] && breaking_cli_changes=true
fi
if [[ -n "$long_after" && "$long_after" != "$long_before" ]]; then
  cli_changes=true
  [[ -n "$removed_long_list" ]] && breaking_cli_changes=true
fi
# Align with current C++ behavior: manual CLI pattern changes also imply CLI changes
if $manual_cli_changes; then
  cli_changes=true
fi

# Also align with C++: any breaking-by-cases implies overall CLI changes
if $breaking_cli_changes; then
  cli_changes=true
fi

# Treat explicit option removals as CLI changes (not automatically breaking)
rsn=${removed_short_count:-0}
rln=${removed_long_count:-0}
mrln=${manual_removed_long_count:-0}
if [[ "$rsn" =~ ^[0-9]+$ ]] && (( rsn > 0 )); then
  cli_changes=true
  # Don't automatically mark option removals as breaking changes
  # They should be treated as minor version bumps, not major
fi
if [[ "$rln" =~ ^[0-9]+$ ]] && (( rln > 0 )); then
  cli_changes=true
  # Don't automatically mark option removals as breaking changes
  # They should be treated as minor version bumps, not major
fi
if [[ "$mrln" =~ ^[0-9]+$ ]] && (( mrln > 0 )); then
  cli_changes=true
  # Don't automatically mark manual option removals as breaking changes
  # They should be treated as minor version bumps, not major
fi

# Synthesize a minimal removed-option signal when switch/case analysis
# indicates breaking CLI removals but we didn't detect explicit option removals.
# This mirrors the C++ analyzer's fallback so the removed-option bonus is applied.
if $breaking_cli_changes; then
  rsc=${removed_short_count:-0}
  rlc=${removed_long_count:-0}
  mrlc=${manual_removed_long_count:-0}
  if [[ "$rsc" =~ ^[0-9]+$ ]] && [[ "$rlc" =~ ^[0-9]+$ ]] && [[ "$mrlc" =~ ^[0-9]+$ ]]; then
    if (( rsc == 0 && rlc == 0 && mrlc == 0 )); then
      removed_long_count=1
    fi
  fi
fi

# Drop heuristic counters to 0 for parity with C++
getopt_changes=0
arg_parsing_changes=0
help_text_changes=0
main_signature_changes=0
enhanced_cli_patterns=0

# --- output functions --------------------------------------------------------

emit_results_zero() {
  note "No relevant source/header changes"
  if $JSON_OUTPUT; then
    cat <<'JSON'
{
  "cli_changes": false,
  "breaking_cli_changes": false,
  "api_breaking": false,
  "manual_cli_changes": false,
  "manual_added_long_count": 0,
  "manual_removed_long_count": 0,
  "removed_short_count": 0,
  "added_short_count": 0,
  "removed_long_count": 0,
  "added_long_count": 0,
  "getopt_changes": 0,
  "arg_parsing_changes": 0,
  "help_text_changes": 0,
  "main_signature_changes": 0,
  "enhanced_cli_patterns": 0,
  "removed_short_list": [],
  "added_short_list": [],
  "removed_long_list": [],
  "added_long_list": []
}
JSON
  elif $MACHINE_OUTPUT; then
    cat <<EOF
CLI_CHANGES=$(boolstr "$cli_changes")
BREAKING_CLI_CHANGES=$(boolstr "$breaking_cli_changes")
API_BREAKING=$(boolstr "$api_breaking")
MANUAL_CLI_CHANGES=$(boolstr "$manual_cli_changes")
MANUAL_ADDED_LONG_COUNT=$manual_added_long_count
MANUAL_REMOVED_LONG_COUNT=$manual_removed_long_count
REMOVED_SHORT_COUNT=$removed_short_count
ADDED_SHORT_COUNT=$added_short_count
REMOVED_LONG_COUNT=$removed_long_count
ADDED_LONG_COUNT=$added_long_count
GETOPT_CHANGES=$getopt_changes
ARG_PARSING_CHANGES=$arg_parsing_changes
HELP_TEXT_CHANGES=$help_text_changes
MAIN_SIGNATURE_CHANGES=$main_signature_changes
ENHANCED_CLI_PATTERNS=$enhanced_cli_patterns
EOF
  else
    printf '=== CLI Options Analysis ===\nNo relevant source/header changes between %s..%s\n' "$BASE_REF" "$TARGET_REF"
  fi
}

emit_json() {
  printf '{\n'
  printf '  "cli_changes": %s,\n' "$(boolstr "$cli_changes")"
  printf '  "breaking_cli_changes": %s,\n' "$(boolstr "$breaking_cli_changes")"
  printf '  "api_breaking": %s,\n' "$(boolstr "$api_breaking")"
  printf '  "manual_cli_changes": %s,\n' "$(boolstr "$manual_cli_changes")"
  printf '  "manual_added_long_count": %d,\n' "$manual_added_long_count"
  printf '  "manual_removed_long_count": %d,\n' "$manual_removed_long_count"
  printf '  "removed_short_count": %d,\n' "$removed_short_count"
  printf '  "added_short_count": %d,\n' "$added_short_count"
  printf '  "removed_long_count": %d,\n' "$removed_long_count"
  printf '  "added_long_count": %d,\n' "$added_long_count"
  printf '  "getopt_changes": %d,\n' "$getopt_changes"
  printf '  "arg_parsing_changes": %d,\n' "$arg_parsing_changes"
  printf '  "help_text_changes": %d,\n' "$help_text_changes"
  printf '  "main_signature_changes": %d,\n' "$main_signature_changes"
  printf '  "enhanced_cli_patterns": %d,\n' "$enhanced_cli_patterns"

  # Lists as arrays
  printf '  "removed_short_list": ['
  first=true; for s in $removed_short_list; do
    $first || printf ', '; first=false; printf '"%s"' "$(json_escape "$s")"
  done; printf '],\n'

  printf '  "added_short_list": ['
  first=true; for s in $added_short_list; do
    $first || printf ', '; first=false; printf '"%s"' "$(json_escape "$s")"
  done; printf '],\n'

  printf '  "removed_long_list": ['
  first=true; while IFS= read -r s; do
    [[ -z "$s" ]] && continue
    $first || printf ', '; first=false; printf '"%s"' "$(json_escape "$s")"
  done <<< "$removed_long_list"; printf '],\n'

  printf '  "added_long_list": ['
  first=true; while IFS= read -r s; do
    [[ -z "$s" ]] && continue
    $first || printf ', '; first=false; printf '"%s"' "$(json_escape "$s")"
  done <<< "$added_long_list"; printf ']\n'
  printf '}\n'
}

emit_machine() {
  cat <<EOF
CLI_CHANGES=$(boolstr "$cli_changes")
BREAKING_CLI_CHANGES=$(boolstr "$breaking_cli_changes")
API_BREAKING=$(boolstr "$api_breaking")
MANUAL_CLI_CHANGES=$(boolstr "$manual_cli_changes")
MANUAL_ADDED_LONG_COUNT=$manual_added_long_count
MANUAL_REMOVED_LONG_COUNT=$manual_removed_long_count
REMOVED_SHORT_COUNT=$removed_short_count
ADDED_SHORT_COUNT=$added_short_count
REMOVED_LONG_COUNT=$removed_long_count
ADDED_LONG_COUNT=$added_long_count
GETOPT_CHANGES=$getopt_changes
ARG_PARSING_CHANGES=$arg_parsing_changes
HELP_TEXT_CHANGES=$help_text_changes
MAIN_SIGNATURE_CHANGES=$main_signature_changes
ENHANCED_CLI_PATTERNS=$enhanced_cli_patterns
EOF
}

emit_human() {
    printf '=== CLI Options Analysis ===\n'
    printf 'Base reference:   %s\n' "$BASE_REF"
    printf 'Target reference: %s\n' "$TARGET_REF"
    printf '\nCLI Changes:\n'
    printf '  CLI interface changes: %s\n' "$(boolstr "$cli_changes")"
    printf '  Breaking CLI changes:  %s\n' "$(boolstr "$breaking_cli_changes")"
    printf '  Manual CLI changes:    %s\n' "$(boolstr "$manual_cli_changes")"
    printf '  API breaking changes:  %s\n' "$(boolstr "$api_breaking")"
    printf '\nOption Counts:\n'
    printf '  Removed short options: %d\n' "$removed_short_count"
    printf '  Added short options:   %d\n' "$added_short_count"
    printf '  Removed long options:  %d\n' "$removed_long_count"
    printf '  Added long options:    %d\n' "$added_long_count"
    printf '  Manual added long:     %d\n' "$manual_added_long_count"
    printf '  Manual removed long:   %d\n' "$manual_removed_long_count"
    if [[ -n "$removed_short_list$added_short_list$removed_long_list$added_long_list" ]]; then
        printf '\nOption Lists:\n'
        [[ -n "$removed_short_list" ]] && { printf '  - Removed short: '; printf '%s\n' "$(printf '%s\n' "$removed_short_list" | one_line)"; }
        [[ -n "$added_short_list"   ]] && { printf '  - Added short:   '; printf '%s\n' "$(printf '%s\n' "$added_short_list" | one_line)"; }
        [[ -n "$removed_long_list"  ]] && { printf '  - Removed long:  '; printf '%s\n' "$(printf '%s\n' "$removed_long_list" | one_line)"; }
        [[ -n "$added_long_list"    ]] && { printf '  - Added long:    '; printf '%s\n' "$(printf '%s\n' "$added_long_list" | one_line)"; }
    fi
    printf '\nPattern Analysis:\n'
    printf '  getopt changes:         %d\n' "$getopt_changes"
    printf '  argument parsing diff:  %d\n' "$arg_parsing_changes"
    printf '  help/usage text diff:   %d\n' "$help_text_changes"
    printf '  main signature changes: %d\n' "$main_signature_changes"
    printf '  enhanced CLI patterns:  %d\n' "$enhanced_cli_patterns"
}

# --- output ------------------------------------------------------------------

if $JSON_OUTPUT; then
  emit_json
elif $MACHINE_OUTPUT; then
  emit_machine
else
  emit_human
fi

# --- exit policy -------------------------------------------------------------

if $FAIL_ON_BREAKING && { $breaking_cli_changes || $api_breaking; }; then
  note "Failing due to breaking changes (--fail-on-breaking)"
  exit 2
fi
