#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Security keyword analyzer for next-version
# Analyzes security-related keywords in git diffs and commit messages

set -Eeuo pipefail
IFS=$'\n\t'

# Source common utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# ------------- defaults -------------
BASE_REF=""
TARGET_REF="HEAD"
REPO_ROOT=""
ONLY_PATHS=""
IGNORE_WHITESPACE=false
ADDED_ONLY=false
NO_MERGES=false
MACHINE_OUTPUT=false
JSON_OUTPUT=false
TOP_COMMITS=10

# Weights (can be tuned via flags)
W_COMMITS=1           # security keywords in commits
W_DIFF_SEC=1          # generic security patterns in diff
W_CVE=3               # CVE references
W_MEM=2               # memory safety issues
W_CRASH=1             # crash/robustness fixes

# ------------- helpers -------------
need() { command -v "$1" >/dev/null 2>&1 || die "$1 not found"; }
# is_true() function is now sourced from version-utils.sh

# Sanitize integer - use is_uint from version-utils.sh instead
# int_or_zero() { printf '%s' "${1:-0}" | tr -cd '0-9'; }

# Sanitize JSON integer emission
emit_json_kv_num() { printf '  "%s": %s' "$1" "$(is_uint "${2:-0}" && printf '%s' "$2" || printf '0')"; }

# GREP engine selection (prefer PCRE for \b, \s, \w, etc.)
grep_supports_pcre() {
    # Busybox grep has no -P; GNU grep usually does. Test once.
    printf 'test' | grep -P '^\w+$' >/dev/null 2>&1
}

# Count occurrences of a regex in stdin (engine-aware)
# $1: pattern
# $2: engine ("pcre" or "ere")
count_occurrences() {
    local pattern="$1" engine="${2:-pcre}"
    local count=0
    if [[ "$engine" == "pcre" ]]; then
        count="$(LC_ALL=C grep -Pio "$pattern" 2>/dev/null | wc -l || printf '0')"
    else
        # ERE fallback (pattern must be ERE-compatible)
        count="$(LC_ALL=C grep -Eio "$pattern" 2>/dev/null | wc -l || printf '0')"
    fi
    # Ensure we return a valid number, defaulting to 0 if tr fails
    printf '%s' "$count" | tr -cd '0-9' 2>/dev/null || printf '0'
}

# Build a git pathspec array from comma-separated globs (supports "!exclude")
# Example: "src,include,!vendor/**"
build_pathspec_array() {
    local spec="${1:-}"
    local -a out=()
    [[ -z "$spec" ]] && { printf '%s\0' ""; return 0; }

    local IFS=,
    read -r -a items <<< "$spec"
    for raw in "${items[@]}"; do
        # trim spaces
        local g="${raw#"${raw%%[![:space:]]*}"}"
        g="${g%"${g##*[![:space:]]}"}"
        [[ -z "$g" ]] && continue
        if [[ "$g" == !* ]]; then
            out+=(":(exclude)${g:1}")
        else
            out+=("$g")
        fi
    done
    ((${#out[@]})) && printf '%s\0' "${out[@]}" || printf '%s\0' ""
}

show_help() {
    cat << EOF
Security Keyword Analyzer

Usage: $(basename "$0") [options]

Options:
  --base <ref>             Base reference for comparison (required)
  --target <ref>           Target reference for comparison (default: HEAD)
  --repo-root <path>       Set repository root directory
  --only-paths <globs>     Comma-separated path globs. Prefix with "!" to exclude.
                           Example: "src,include,!third_party/**"
  --ignore-whitespace      Ignore whitespace changes in diff analysis
  --added-only             Scan only added lines in diffs (skip context and removals)
  --no-merges              Ignore merge commits in commit-message scan
  --top <N>                Show up to N commit subjects in human output (default: 10)
  --machine                Output machine-readable key=value format
  --json                   Output machine-readable JSON

Weights:
  --w-commits <n>          Weight for commit keywords (default: $W_COMMITS)
  --w-diff-sec <n>         Weight for diff security patterns (default: $W_DIFF_SEC)
  --w-cve <n>              Weight for CVE matches (default: $W_CVE)
  --w-mem <n>              Weight for memory safety issues (default: $W_MEM)
  --w-crash <n>            Weight for crash/robustness fixes (default: $W_CRASH)

Examples:
  $(basename "$0") --base v1.0.0 --target HEAD
  $(basename "$0") --base HEAD~5 --target HEAD --machine
  $(basename "$0") --base v1.0.0 --target v1.1.0 --json
  $(basename "$0") --base v1.0.0 --target HEAD --only-paths "src,include,!vendor/**" --added-only
EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --base)           BASE_REF="${2:?}"; shift 2 ;;
        --target)         TARGET_REF="${2:?}"; shift 2 ;;
        --repo-root)      REPO_ROOT="${2:?}"; shift 2 ;;
        --only-paths)     ONLY_PATHS="${2:?}"; shift 2 ;;
        --ignore-whitespace) IGNORE_WHITESPACE=true; shift ;;
        --added-only)     ADDED_ONLY=true; shift ;;
        --no-merges)      NO_MERGES=true; shift ;;
        --top)            TOP_COMMITS="${2:?}"; shift 2 ;;
        --machine)        MACHINE_OUTPUT=true; shift ;;
        --json)           JSON_OUTPUT=true; shift ;;
        --w-commits)      W_COMMITS="${2:?}"; shift 2 ;;
        --w-diff-sec)     W_DIFF_SEC="${2:?}"; shift 2 ;;
        --w-cve)          W_CVE="${2:?}"; shift 2 ;;
        --w-mem)          W_MEM="${2:?}"; shift 2 ;;
        --w-crash)        W_CRASH="${2:?}"; shift 2 ;;
        --help|-h)        show_help; exit 0 ;;
        *) die "Unknown option: $1" ;;
    esac
done

# Validate required arguments
if [[ -z "$BASE_REF" ]]; then
    printf 'Error: --base is required\n' >&2
    exit 1
fi

# Check required commands
need git
need grep
need wc
need tr

# Change to repo root if specified
if [[ -n "$REPO_ROOT" ]]; then
    cd "$REPO_ROOT"
fi
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "Not inside a git repository"

# Build PATH_ARGS for git commands
PATH_ARGS=()
# read -d '' stops at NUL; mapfile/readarray required for safe parsing
if mapfile -d '' _pspec < <(build_pathspec_array "$ONLY_PATHS"); then
    if ((${#_pspec[@]})) && [[ -n "${_pspec[0]}" ]]; then
        PATH_ARGS=(-- "${_pspec[@]}")
    fi
fi

# Verify git reference exists
verify_ref() {
    local ref="$1"
    git rev-parse --verify --quiet "${ref}^{commit}" >/dev/null || die "Invalid reference: $ref"
}

verify_ref "$BASE_REF"
verify_ref "$TARGET_REF"

# ------------- patterns (regex) -------------
# Default to ERE for portability and stability. PCRE (-P) can exhibit
# environment-specific failures with complex patterns. Allow opt-in via
# FORCE_PCRE=true if absolutely needed and supported.
USE_PCRE=false
if [[ "${FORCE_PCRE:-}" == "true" ]] && grep_supports_pcre; then
    USE_PCRE=true
fi

if $USE_PCRE; then
    # PCRE patterns
    SEC_REGEX='(?ix)
        \b(
          security | vuln(?:erab\w*)? | exploit | breach | attack | threat
          | malware | virus | trojan | backdoor | rootkit | phishing | spam
          | ddos | \bdo[sz]\b
          | overflow | injection | xss | csrf | sqli | rce | ssrf | xxe
          | privilege | escalation | bypass | circumvent
          | mitigation | hardening | sandbox | policy
          | auth(?:entication|orization)? | session(?:\s*hijack\w*)?
          | crypto(?:graphy)? | encryption | decryption | tls | ssl | x\.509 | certificate(?:\s*pinning)?
          | secret | token | leak | expos(?:e|ure)
          | path\s*traversal | directory\s*traversal
        )\b
    '
    CVE_REGEX='(?i)\bCVE-[0-9]{4}-[0-9]{4,7}\b'
    MEM_REGEX='(?ix)
        \b(
          buffer[- _]?overflow | stack[- _]?overflow | heap[- _]?overflow
          | use[- _]?after[- _]?free | double[- _]?free
          | null[- _]?pointer | dangling[- _]?pointer
          | out[- _]?of[- _]?bounds | oob
          | memory[- _]?leak | format[- _]?string | integer[- _]?overflow | signedness
          | race[- _]?condition | data[- _]?race | deadlock
        )\b
    '
    CRASH_REGEX='(?ix)
        \b(
          segfault | segmentation\s+fault | crash | abort | assert | panic
          | fatal(?:\s+error)? | core\s+dump | stack\s+trace
        )\b
    '
    GREP_ENGINE="pcre"
else
    # ERE fallback (approximate \b with (^|non-word) ... (non-word|$), \s with [[:space:]])
    SEC_REGEX='((^|[^[:alnum:]_])(security|vuln(ererab[^[:space:]]*)?|exploit|breach|attack|threat|malware|virus|trojan|backdoor|rootkit|phishing|spam|ddos|do[sz]|overflow|injection|xss|csrf|sqli|rce|ssrf|xxe|privilege|escalation|bypass|circumvent|mitigation|hardening|sandbox|policy|auth(entication|orization)?|session([[:space:]]*hijack[^[:space:]]*)?|crypto(graphy)?|encryption|decryption|tls|ssl|x\.509|certificate([[:space:]]*pinning)?|secret|token|leak|expos(e|ure)|path[[:space:]]*traversal|directory[[:space:]]*traversal))([^[:alnum:]_]|$))'
    CVE_REGEX='CVE-[0-9]{4}-[0-9]{4,7}'
    MEM_REGEX='((^|[^[:alnum:]_])(buffer[- _]?overflow|stack[- _]?overflow|heap[- _]?overflow|use[- _]?after[- _]?free|double[- _]?free|null[- _]?pointer|dangling[- _]?pointer|out[- _]?of[- _]?bounds|oob|memory[- _]?leak|format[- _]?string|integer[- _]?overflow|signedness|race[- _]?condition|data[- _]?race|deadlock)([^[:alnum:]_]|$))'
    CRASH_REGEX='((^|[^[:alnum:]_])(segfault|segmentation[[:space:]]+fault|crash|abort|assert|panic|fatal([[:space:]]+error)?|core[[:space:]]+dump|stack[[:space:]]+trace)([^[:alnum:]_]|$))'
    GREP_ENGINE="ere"
fi

# Analyze security keywords
analyze_security_keywords() {
    local base_ref="$1"
    local target_ref="$2"
    
    # Commit message scan (subjects + bodies to catch CVEs etc.)
    local -a log_args=(log --format=%s%n%b)
    $NO_MERGES && log_args=(--no-merges "${log_args[@]}")
    local commits_text
    commits_text="$(git -c color.ui=false "${log_args[@]}" "$base_ref..$target_ref" 2>/dev/null || true)"

    # Count security-related keywords in commit subjects+bodies. Use a robust
    # ERE pattern for commit scanning to avoid edge-cases with PCRE/newlines,
    # aligning behavior with the C++ analyzer that scans commits broadly.
    # This complements (and is separately reported from) diff-based signals.
    local security_keywords=0
    if [[ -n "$commits_text" ]]; then
        # Commit keyword pattern (matches SECURITY, VULNERABILITY/IES, CVE-YYYY-NNNN)
        local COMMIT_SEC_REGEX='(SECURITY|VULNERABILIT(Y|IES)|CVE[- ]?[0-9]{4}-[0-9]+)'
        security_keywords="$(printf '%s' "$commits_text" | grep -Eio "$COMMIT_SEC_REGEX" 2>/dev/null | wc -l | tr -d ' ' || printf '0')"
    fi
    security_keywords="$(is_uint "$security_keywords" && printf '%s' "$security_keywords" || printf '0')"
    
    # Diff scan (with options)
    local -a diff_args=(diff -M -C "$base_ref..$target_ref")
    $IGNORE_WHITESPACE && diff_args+=( -w )
    # Narrow context when only added lines requested
    $ADDED_ONLY && diff_args+=( -U0 )

    # fetch diff
    local diff_content
    if ((${#PATH_ARGS[@]})); then
        diff_content="$(git -c color.ui=false "${diff_args[@]}" "${PATH_ARGS[@]}" 2>/dev/null || true)"
    else
        diff_content="$(git -c color.ui=false "${diff_args[@]}" 2>/dev/null || true)"
    fi

    # Keep only added lines if requested (+ but not +++ header)
    if $ADDED_ONLY && [[ -n "$diff_content" ]]; then
        # remove file headers and hunk markers; keep lines beginning with single '+'
        diff_content="$(printf '%s\n' "$diff_content" \
            | grep -Ev '^(--- |\+\+\+ |@@ )' \
            | grep -E '^\+[^+]' \
            | sed 's/^+//')"
    fi

    local security_patterns=0 cve_patterns=0 memory_safety_issues=0 crash_fixes=0
    if [[ -n "$diff_content" ]]; then
        security_patterns="$(printf '%s' "$diff_content" | count_occurrences "$SEC_REGEX" "$GREP_ENGINE")"
        cve_patterns="$(printf '%s' "$diff_content" | count_occurrences "$CVE_REGEX" "$GREP_ENGINE")"
        memory_safety_issues="$(printf '%s' "$diff_content" | count_occurrences "$MEM_REGEX" "$GREP_ENGINE")"
        crash_fixes="$(printf '%s' "$diff_content" | count_occurrences "$CRASH_REGEX" "$GREP_ENGINE")"
    fi

    security_patterns="$(is_uint "$security_patterns" && printf '%s' "$security_patterns" || printf '0')"
    cve_patterns="$(is_uint "$cve_patterns" && printf '%s' "$cve_patterns" || printf '0')"
    memory_safety_issues="$(is_uint "$memory_safety_issues" && printf '%s' "$memory_safety_issues" || printf '0')"
    crash_fixes="$(is_uint "$crash_fixes" && printf '%s' "$crash_fixes" || printf '0')"
    
    # Calculate weighted total security score
    local total_security_score=$(( security_keywords * W_COMMITS \
                                   + security_patterns * W_DIFF_SEC \
                                   + cve_patterns * W_CVE \
                                   + memory_safety_issues * W_MEM \
                                   + crash_fixes * W_CRASH ))

    # Simple normalized risk band (heuristic, stable across repos)
    # 0 => none, 1-4 low, 5-14 medium, 15+ high (you can tune later)
    local risk="none"
    if   (( total_security_score >= 15 )); then risk="high"
    elif (( total_security_score >= 5  )); then risk="medium"
    elif (( total_security_score >= 1  )); then risk="low"
    fi
    
    # Output results
    if [[ "$JSON_OUTPUT" = "true" ]]; then
        printf '{\n'
        emit_json_kv_num "security_keywords" "$security_keywords"; printf ',\n'
        emit_json_kv_num "security_patterns" "$security_patterns"; printf ',\n'
        emit_json_kv_num "cve_patterns" "$cve_patterns"; printf ',\n'
        emit_json_kv_num "memory_safety_issues" "$memory_safety_issues"; printf ',\n'
        emit_json_kv_num "crash_fixes" "$crash_fixes"; printf ',\n'
        emit_json_kv_num "total_security_score" "$total_security_score"; printf ',\n'
        printf '  "risk": "%s",\n' "$risk"
        printf '  "weights": {\n'
        emit_json_kv_num "commits" "$W_COMMITS";          printf ',\n'
        emit_json_kv_num "diff_security" "$W_DIFF_SEC";   printf ',\n'
        emit_json_kv_num "cve" "$W_CVE";                  printf ',\n'
        emit_json_kv_num "memory" "$W_MEM";               printf ',\n'
        emit_json_kv_num "crash" "$W_CRASH";              printf '\n'
        printf '  },\n'
        printf '  "engine": "%s"\n' "$GREP_ENGINE"
        printf '}\n'
    elif [[ "$MACHINE_OUTPUT" = "true" ]]; then
        printf 'SECURITY_KEYWORDS=%s\n' "$security_keywords"
        printf 'SECURITY_PATTERNS=%s\n' "$security_patterns"
        printf 'CVE_PATTERNS=%s\n' "$cve_patterns"
        printf 'MEMORY_SAFETY_ISSUES=%s\n' "$memory_safety_issues"
        printf 'CRASH_FIXES=%s\n' "$crash_fixes"
        printf 'TOTAL_SECURITY_SCORE=%s\n' "$total_security_score"
        printf 'RISK=%s\n' "$risk"
        printf 'WEIGHT_COMMITS=%s\n' "$W_COMMITS"
        printf 'WEIGHT_DIFF_SEC=%s\n' "$W_DIFF_SEC"
        printf 'WEIGHT_CVE=%s\n' "$W_CVE"
        printf 'WEIGHT_MEMORY=%s\n' "$W_MEM"
        printf 'WEIGHT_CRASH=%s\n' "$W_CRASH"
        printf 'ENGINE=%s\n' "$GREP_ENGINE"
    else
        printf '=== Security Keyword Analysis ===\n'
        printf 'Base reference : %s\n' "$base_ref"
        printf 'Target reference: %s\n' "$target_ref"
        printf 'Engine         : %s\n' "$GREP_ENGINE"
        printf '\nSecurity Analysis:\n'
        printf '  Security keywords in commits: %s (w=%s)\n' "$security_keywords" "$W_COMMITS"
        printf '  Security patterns in code   : %s (w=%s)\n' "$security_patterns" "$W_DIFF_SEC"
        printf '  CVE references              : %s (w=%s)\n' "$cve_patterns" "$W_CVE"
        printf '  Memory safety issues        : %s (w=%s)\n' "$memory_safety_issues" "$W_MEM"
        printf '  Crash fixes                 : %s (w=%s)\n' "$crash_fixes" "$W_CRASH"
        printf '  -------------------------------------\n'
        printf '  Total security score        : %s\n' "$total_security_score"
        printf '  Risk level                  : %s\n' "$risk"
        
        if [[ "$total_security_score" -gt 0 ]]; then
            printf '\nMatching commit subjects (top %d):\n' "$TOP_COMMITS"
            # show only subjects, filtered by security regex
            local -a log_subject_args=(log --format=%s)
            $NO_MERGES && log_subject_args=(--no-merges "${log_subject_args[@]}")
            
            # Use engine explicitly for filtering:
            if [[ "$GREP_ENGINE" == "pcre" ]]; then
                git -c color.ui=false "${log_subject_args[@]}" "$base_ref..$target_ref" \
                    | grep -Pi "$SEC_REGEX" \
                    | head -n "$TOP_COMMITS" || printf '  (none)\n'
            else
                git -c color.ui=false "${log_subject_args[@]}" "$base_ref..$target_ref" \
                    | grep -Ei "$SEC_REGEX" \
                    | head -n "$TOP_COMMITS" || printf '  (none)\n'
            fi
        fi
    fi
}

# Main execution
analyze_security_keywords "$BASE_REF" "$TARGET_REF"
