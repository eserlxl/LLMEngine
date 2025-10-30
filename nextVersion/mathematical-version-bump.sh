#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Mathematical version bump for next-version
# Performs mathematical version bumping with git operations

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C

# Source common utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# ------------------------------ bootstrap ------------------------------------

readonly PROG=${0##*/}

# realpath is required before SCRIPT_DIR is known
if ! command -v realpath >/dev/null 2>&1; then
  echo "[$PROG] Error: realpath not found." >&2
  exit 127
fi

# Helper to die consistently (before version-utils is sourced)
# _die_boot() { printf '[%s] %s\n' "$PROG" "$*" >&2; exit 1; }

# Require Bash ≥ 4 (associative arrays & modern features used in utilities)
if (( BASH_VERSINFO[0] < 4 )); then
  die "Bash ≥ 4 is required; current: ${BASH_VERSION}"
fi

# Validate helper scripts exist early (fail fast & clear)
_need_exec() { local p="$1" name="${2:-$1}"; [[ -x "$p" ]] || die "Required helper '$name' not executable at: $p"; }
[[ -r "$SCRIPT_DIR/version-utils.sh" ]] || die "Missing: $SCRIPT_DIR/version-utils.sh"
_need_exec "$SCRIPT_DIR/semantic-version-analyzer.sh" "semantic-version-analyzer"
_need_exec "$SCRIPT_DIR/version-calculator-loc.sh"      "version-calculator-loc"
_need_exec "$SCRIPT_DIR/git-operations.sh"              "git-operations"
_need_exec "$SCRIPT_DIR/version-validator.sh"           "version-validator"

# shellcheck source=/dev/null
# shellcheck disable=SC1091
# shellcheck disable=SC2154
source "$SCRIPT_DIR/version-utils.sh"

# ------------------------------ traps/cleanup --------------------------------

# Ensure cleanup hook exists even if utilities change later
# Note: cleanup is handled by version-utils.sh automatically

# Predefine for shellcheck; real value captured in trap
st=0

# shellcheck disable=SC2154
trap '{
  st=$?
  if (( st != 0 )); then
    warn "Aborted with status $st at line ${BASH_LINENO[0]} executing: ${BASH_COMMAND}"
  fi
  # cleanup provided by version-utils (register_tmp)
}' EXIT

# ------------------------------ global options --------------------------------
# Defaults (env can still override these before parse)
OPT_SET_VERSION=""
OPT_DO_COMMIT="false"
OPT_DO_TAG="false"
OPT_DO_PUSH="false"
OPT_PUSH_TAGS="false"
OPT_COMMIT_MSG=""
OPT_NO_VERIFY="false"
OPT_COMMIT_SIGN="false"
OPT_TAG_PREFIX="${TAG_PREFIX:-v}"
OPT_ANNOTATED_TAG="true"
OPT_SIGNED_TAG="false"
OPT_ALLOW_DIRTY="false"
OPT_REMOTE="${REMOTE:-origin}"
OPT_ALLOW_NONMONOTONIC_TAG="${ALLOW_NONMONOTONIC_TAG:-false}"
OPT_PRINT_ONLY="false"
OPT_DRY_RUN="false"
OPT_ALLOW_PRERELEASE="false"
OPT_REPO_ROOT="${REPO_ROOT:-}"
OPT_NO_COLOR="${NO_COLOR:-false}"

# Analyzer scope
OPT_SINCE_TAG=""
OPT_SINCE_COMMIT=""
OPT_SINCE_DATE=""
OPT_BASE_REF=""
OPT_TARGET_REF=""
OPT_NO_MERGE_BASE="false"
OPT_ONLY_PATHS=""
OPT_IGNORE_WHITESPACE="false"

ORIGINAL_PROJECT_ROOT=""  # filled after resolve_script_paths

# ------------------------------ setup -----------------------------------------
setup_environment() {
  init_colors "$OPT_NO_COLOR"
  resolve_script_paths "$0" "$OPT_REPO_ROOT"
  ORIGINAL_PROJECT_ROOT="$PROJECT_ROOT"
  require_cmd git grep sed awk tr realpath
}

# ------------------------------ CLI parsing -----------------------------------
parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --set)               [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--set requires a value"; OPT_SET_VERSION="$2"; shift 2 ;;
      --commit)            OPT_DO_COMMIT="true"; shift ;;
      --tag)               OPT_DO_TAG="true"; shift ;;
      --push)              OPT_DO_PUSH="true"; shift ;;
      --push-tags)         OPT_PUSH_TAGS="true"; shift ;;
      --message)           [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--message requires a value"; OPT_COMMIT_MSG="$2"; shift 2 ;;
      --allow-dirty)       OPT_ALLOW_DIRTY="true"; shift ;;
      --lightweight-tag)   OPT_ANNOTATED_TAG="false"; shift ;;
      --signed-tag)        OPT_SIGNED_TAG="true"; shift ;;
      --no-verify)         OPT_NO_VERIFY="true"; shift ;;
      --sign-commit)       OPT_COMMIT_SIGN="true"; shift ;;
      --tag-prefix)        [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--tag-prefix requires a value"; OPT_TAG_PREFIX="$2"; shift 2 ;;
      --no-color)          OPT_NO_COLOR="true"; shift ;;
      --print)             OPT_PRINT_ONLY="true"; shift ;;
      --dry-run)           OPT_DRY_RUN="true"; shift ;;
      --allow-prerelease)  OPT_ALLOW_PRERELEASE="true"; shift ;;
      --repo-root)         [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--repo-root requires a value"; OPT_REPO_ROOT="$2"; shift 2 ;;
      --remote)            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--remote requires a value"; OPT_REMOTE="$2"; shift 2 ;;
      --allow-nonmonotonic-tag) OPT_ALLOW_NONMONOTONIC_TAG="true"; shift ;;

      # Analyzer options
      --since)             [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--since requires a value"; OPT_SINCE_TAG="$2"; shift 2 ;;
      --since-commit)      [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--since-commit requires a value"; OPT_SINCE_COMMIT="$2"; shift 2 ;;
      --since-date)        [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--since-date requires a value"; OPT_SINCE_DATE="$2"; shift 2 ;;
      --base)              [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--base requires a value"; OPT_BASE_REF="$2"; shift 2 ;;
      --target)            [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--target requires a value"; OPT_TARGET_REF="$2"; shift 2 ;;
      --no-merge-base)     OPT_NO_MERGE_BASE="true"; shift ;;
      --only-paths)        [[ -n "${2-}" && "${2#-}" = "$2" ]] || die "--only-paths requires a value"; OPT_ONLY_PATHS="$2"; shift 2 ;;
      --ignore-whitespace) OPT_IGNORE_WHITESPACE="true"; shift ;;
      --help|-h)           show_help; exit 0 ;;
      *)                   die "Unknown option: $1" ;;
    esac
  done
}

# ------------------------------ help ------------------------------------------
show_help() {
  cat << 'EOF'
Mathematical Version Bumper for next-version

Usage: ./bin/mathematical-version-bump.sh [options]

Purely mathematical versioning system: determines bump from semantic analysis.

Options:
  --set VERSION              Set version directly (X.Y.Z or prerelease with --allow-prerelease)
  --commit                   Create a commit with version changes
  --tag                      Create a git tag for the new version
  --push                     Push commits to remote
  --push-tags                Push tags to remote
  --message MSG              Custom commit message
  --allow-dirty              Allow version bump on dirty working tree
  --lightweight-tag          Create lightweight tag instead of annotated
  --signed-tag               Create signed tag
  --no-verify                Skip git hooks
  --sign-commit              Sign the commit
  --tag-prefix PREFIX        Tag prefix (default: v)
  --no-color                 Disable colored output
  --print                    Print computed version and exit
  --dry-run                  Show what would be done without doing it
  --allow-prerelease         Allow prerelease with --set
  --repo-root PATH           Repository root directory
  --remote REMOTE            Remote name (default: origin)
  --allow-nonmonotonic-tag   Allow non-monotonic tags

Analysis Options (forwarded to semantic analyzer):
  --since TAG                Analyze changes since tag
  --since-commit HASH        Analyze changes since commit
  --since-date DATE          Analyze changes since date (YYYY-MM-DD)
  --base REF                 Base reference for comparison
  --target REF               Target reference for comparison
  --no-merge-base            Disable automatic merge-base detection
  --only-paths GLOBS         Restrict to comma-separated path globs
  --ignore-whitespace        Ignore whitespace-only diffs

Examples:
  ./bin/mathematical-version-bump.sh --dry-run
  ./bin/mathematical-version-bump.sh --commit --tag
  ./bin/mathematical-version-bump.sh --set 1.0.0 --allow-prerelease
  ./bin/mathematical-version-bump.sh --since v1.0.0 --commit
  ./bin/mathematical-version-bump.sh --print
EOF
}

# --------------------------- version file ops ---------------------------------
handle_version_file() {
  local version_exists="false" current_version="none"

  if [[ -f "$VERSION_FILE" ]]; then
    version_exists="true"
    validate_version_file_path "$VERSION_FILE" "$PROJECT_ROOT"
    current_version="$(read_version_file "$VERSION_FILE")"
    if ! is_semver "$current_version"; then
      printf '%s\n' "${YELLOW}Expected: MAJOR.MINOR.PATCH (optionally with prerelease/build when allowed)${RESET}" >&2
      die "Invalid format in VERSION ($VERSION_FILE): '$current_version'"
    fi
  elif [[ -n "$OPT_SET_VERSION" ]]; then
    printf '%s\n' "${YELLOW}Note: VERSION not found; will create with ${OPT_SET_VERSION}${RESET}" >&2
  else
    printf '%s\n' "${YELLOW}Provide --set VERSION to create it, or add a VERSION file${RESET}" >&2
    die "VERSION file not found at: $VERSION_FILE"
  fi

  printf '%s\n%s' "$version_exists" "$current_version"
}

# ---------------------- mathematical version calculation ----------------------
_valid_bump_type() {
  case "$1" in major|minor|patch|none) return 0 ;; *) return 1 ;; esac
}

_is_prerelease() { [[ "$1" == *-* ]]; }

calculate_mathematical_version() {
  local current_version="$1"

  # Direct set
  if [[ -n "$OPT_SET_VERSION" ]]; then
    validate_version_format "$OPT_SET_VERSION" "$OPT_ALLOW_PRERELEASE"
    printf '%s' "$OPT_SET_VERSION"
    return 0
  fi

  printf '%s\n' "${CYAN}Analyzing changes to determine mathematical version bump...${RESET}" >&2

  local -a analyzer_args=( --json )
  [[ -n "$OPT_REPO_ROOT"     ]] && analyzer_args+=( --repo-root "$OPT_REPO_ROOT" )
  [[ -n "$OPT_SINCE_TAG"     ]] && analyzer_args+=( --since "$OPT_SINCE_TAG" )
  [[ -n "$OPT_SINCE_COMMIT"  ]] && analyzer_args+=( --since-commit "$OPT_SINCE_COMMIT" )
  [[ -n "$OPT_SINCE_DATE"    ]] && analyzer_args+=( --since-date "$OPT_SINCE_DATE" )
  [[ -n "$OPT_BASE_REF"      ]] && analyzer_args+=( --base "$OPT_BASE_REF" )
  [[ -n "$OPT_TARGET_REF"    ]] && analyzer_args+=( --target "$OPT_TARGET_REF" )
  [[ "$OPT_NO_MERGE_BASE" == "true" ]] && analyzer_args+=( --no-merge-base )
  [[ -n "$OPT_ONLY_PATHS"    ]] && analyzer_args+=( --only-paths "$OPT_ONLY_PATHS" )
  [[ "$OPT_IGNORE_WHITESPACE" == "true" ]] && analyzer_args+=( --ignore-whitespace )

  local sa_output="" sa_rc=0
  # Use temporary file to capture output without triggering error handling
  local temp_output
  temp_output="$(mktemp /tmp/nv.sa.XXXXXX)"
  # Temporarily disable ERR trap so non-zero analyzer exit codes don't abort
  local __old_err_trap
  __old_err_trap="$(trap -p ERR || true)"
  trap - ERR
  set +e
  "$SCRIPT_DIR/semantic-version-analyzer.sh" "${analyzer_args[@]}" > "$temp_output" 2> >(sed 's/^/[analyzer] /' >&2)
  sa_rc=$?
  set -e
  # Restore previous ERR trap (if any)
  if [[ -n "$__old_err_trap" ]]; then
    eval "$__old_err_trap"
  else
    trap - ERR
  fi
  sa_output="$(cat "$temp_output" 2>/dev/null || echo "")"
  

  
  rm -f "$temp_output"
  
  # Semantic analyzer uses exit codes to indicate bump type, not errors
  # 10=major, 11=minor, 12=patch, 20=none, 0=success
  if [[ "$sa_rc" -ne 0 && "$sa_rc" -ne 10 && "$sa_rc" -ne 11 && "$sa_rc" -ne 12 && "$sa_rc" -ne 20 ]]; then
    warn "semantic-version-analyzer exited with unexpected status $sa_rc"
  fi

  # Parse JSON output to get suggestion and next_version
  local suggested_bump="none"
  local next_version=""
  
  # Prefer JSON fields when available and jq exists
  if [[ -n "$sa_output" && $(command -v jq >/dev/null 2>&1; printf %s $?) -eq 0 ]]; then
    suggested_bump="$(echo "$sa_output" | jq -r '.suggestion // "none"' 2>/dev/null || echo "none")"
    next_version="$(echo "$sa_output" | jq -r '.next_version // empty' 2>/dev/null || echo "")"
  fi

  # If JSON parsing failed or produced empty suggestion, map from exit code
  if [[ -z "$suggested_bump" || "$suggested_bump" == "none" ]]; then
    case "$sa_rc" in
      10) suggested_bump="major" ;;
      11) suggested_bump="minor" ;;
      12) suggested_bump="patch" ;;
      20) suggested_bump="none"  ;;
    esac
  fi

  if [[ -z "$suggested_bump" ]]; then
    suggested_bump="none"
  fi
  if ! _valid_bump_type "$suggested_bump"; then
    warn "Analyzer returned unknown bump '$suggested_bump'; treating as 'none'."
    suggested_bump="none"
  fi

  if [[ "$suggested_bump" == "none" ]]; then
    printf '%s\n' "${YELLOW}No qualifying changes detected; version unchanged.${RESET}" >&2
    printf '%s' "$current_version"
    return 0
  fi

  printf '%s\n' "${CYAN}Mathematical analysis suggests: ${suggested_bump} bump${RESET}" >&2

  # Use the next_version from semantic analyzer if available, otherwise fall back to version-calculator-loc.sh
  if [[ -n "$next_version" ]]; then
    printf '%s' "$next_version"
  else
    local -a calculator_args=(
      --current-version "$current_version"
      --bump-type "$suggested_bump"
      --original-project-root "$ORIGINAL_PROJECT_ROOT"
    )
    [[ -n "$OPT_REPO_ROOT" ]] && calculator_args+=( --repo-root "$OPT_REPO_ROOT" )

    local new_version
    new_version="$("$SCRIPT_DIR/version-calculator-loc.sh" "${calculator_args[@]}")"
    printf '%s' "$new_version"
  fi
}

# ------------------------------ file updates ----------------------------------
update_files() {
  local new_version="$1" current_version="$2"

  if _is_prerelease "$new_version"; then
    warn "Pre-release $new_version — skipping write to VERSION"
    printf '%s\n' "${CYAN}Note: Pre-release identifiers are not stored in VERSION.${RESET}" >&2
  else
    safe_write_file "$VERSION_FILE" "$new_version"
    if [[ "$current_version" == "none" ]]; then
      ok "Created VERSION: $new_version"
    else
      ok "Updated VERSION: $current_version → $new_version"
    fi
  fi

  # If CMakeLists.txt reads from VERSION, nothing to do here.
  if [[ "$OPT_DRY_RUN" == "true" ]]; then
    printf '%s\n' "${YELLOW}DRY RUN: CMakeLists.txt auto-updates from VERSION${RESET}" >&2
  fi
}

# ------------------------------ git ops ---------------------------------------
perform_git_operations() {
  local new_version="$1" current_version="$2"

  # Effective toggles: never tag/commit on prerelease (no file change)
  local eff_do_commit="$OPT_DO_COMMIT"
  local eff_do_tag="$OPT_DO_TAG"
  if _is_prerelease "$new_version"; then
    if [[ "$OPT_DO_TAG" == "true" ]]; then
      warn "Blocking tag creation for prerelease $new_version"
    fi
    eff_do_tag="false"
    # Commit would likely be empty; block to avoid no-op failures
    if [[ "$OPT_DO_COMMIT" == "true" ]]; then
      warn "Skipping commit for prerelease $new_version (no persistent file changes)"
    fi
    eff_do_commit="false"
  fi

  if [[ "$eff_do_commit" == "true" || "$eff_do_tag" == "true" || "$OPT_DO_PUSH" == "true" || "$OPT_PUSH_TAGS" == "true" ]]; then
    "$SCRIPT_DIR/git-operations.sh" perform-git-operations \
      "$VERSION_FILE" \
      "false" \
      "$new_version" \
      "$current_version" \
      "$eff_do_commit" \
      "$eff_do_tag" \
      "$OPT_DO_PUSH" \
      "$OPT_PUSH_TAGS" \
      "$OPT_COMMIT_MSG" \
      "$OPT_NO_VERIFY" \
      "$OPT_COMMIT_SIGN" \
      "$OPT_TAG_PREFIX" \
      "$OPT_ANNOTATED_TAG" \
      "$OPT_SIGNED_TAG" \
      "$OPT_ALLOW_DIRTY" \
      "$PROJECT_ROOT" \
      "$ORIGINAL_PROJECT_ROOT" \
      "$OPT_REMOTE" \
      "$OPT_ALLOW_NONMONOTONIC_TAG"
  fi
}

# ------------------------------ dry run ---------------------------------------
simulate_dry_run() {
  local new_version="$1" current_version="$2" version_exists="$3"

  if [[ "$current_version" == "none" ]]; then
    if _is_prerelease "$new_version"; then
      printf '%s\n' "${YELLOW}DRY RUN: Would print prerelease $new_version (no VERSION write)${RESET}" >&2
    else
      printf '%s\n' "${YELLOW}DRY RUN: Would create VERSION with $new_version${RESET}" >&2
    fi
  else
    if _is_prerelease "$new_version"; then
      printf '%s\n' "${YELLOW}DRY RUN: Would print prerelease $new_version (no VERSION update)${RESET}" >&2
    else
      printf '%s\n' "${YELLOW}DRY RUN: Would update VERSION to $new_version${RESET}" >&2
    fi
  fi

  if [[ "$OPT_DO_COMMIT" == "true" ]]; then
    if _is_prerelease "$new_version"; then
      printf '%s\n' "${YELLOW}DRY RUN: Would skip commit (no persistent file changes for prerelease)${RESET}" >&2
    else
      printf '%s\n' "${YELLOW}DRY RUN: Would create commit: chore(release): ${OPT_TAG_PREFIX}${new_version}${RESET}" >&2
      printf '%s\n' "${YELLOW}DRY RUN: Would commit files: VERSION${RESET}" >&2
    fi
  fi

  if [[ "$OPT_DO_TAG" == "true" ]]; then
    if _is_prerelease "$new_version"; then
      printf '%s\n' "${YELLOW}DRY RUN: Would block tag creation for prerelease $new_version${RESET}" >&2
    else
      printf '%s\n' "${YELLOW}DRY RUN: Would create tag: ${OPT_TAG_PREFIX}${new_version}${RESET}" >&2
    fi
    local last_tag; last_tag="$("$SCRIPT_DIR/version-utils.sh" last-tag "$OPT_TAG_PREFIX" || true)"
    [[ -n "$last_tag" ]] && printf '%s\n' "${YELLOW}DRY RUN: Last tag for comparison: $last_tag${RESET}" >&2
  fi

  printf '%s\n' "$new_version"
}

# ----------------------------- print-only mode --------------------------------
handle_print_only() {
  if [[ -n "$OPT_SET_VERSION" ]]; then
    validate_version_format "$OPT_SET_VERSION" "$OPT_ALLOW_PRERELEASE"
    printf '%s\n' "$OPT_SET_VERSION"
    exit 0
  elif [[ -f "$VERSION_FILE" ]]; then
    local current_version; current_version="$(read_version_file "$VERSION_FILE")"
    if is_semver "$current_version"; then
      calculate_mathematical_version "$current_version"; echo; exit 0
    fi
  fi
  die "Cannot compute version for --print. Provide --set VERSION or ensure VERSION exists and is valid."
}

# ------------------------------ validations -----------------------------------
validate_options() {
  # Pushing implies there is a remote
  if [[ "$OPT_DO_PUSH" == "true" || "$OPT_PUSH_TAGS" == "true" ]]; then
    [[ -n "$OPT_REMOTE" ]] || die "--push/--push-tags requires a remote (use --remote)"
  fi
}

# ---------------------------------- main --------------------------------------
main() {
  parse_args "$@"
  setup_environment
  validate_options

  if [[ "$OPT_PRINT_ONLY" == "true" ]]; then
    handle_print_only
  fi

  # VERSION file handling
  local version_info version_exists current_version
  version_info="$(handle_version_file)"
  version_exists="${version_info%$'\n'*}"
  current_version="${version_info#*$'\n'}"

  # Compute new version using mathematical analysis
  local new_version
  new_version="$(calculate_mathematical_version "$current_version")"

  # No-op guard
  if [[ "$current_version" == "$new_version" ]]; then
    printf '%s\n' "${YELLOW}Version unchanged ($current_version); nothing to do.${RESET}" >&2
    printf '%s\n' "$new_version"
    return 0
  fi

  # Dry-run path
  if [[ "$OPT_DRY_RUN" == "true" ]]; then
    simulate_dry_run "$new_version" "$current_version" "$version_exists"
    return 0
  fi

  # Apply updates and git actions
  update_files "$new_version" "$current_version"
  perform_git_operations "$new_version" "$current_version"

  # Success & next steps
  if [[ "$current_version" == "none" ]]; then
    ok "Mathematical version bump completed: created $new_version"
  else
    ok "Mathematical version bump completed: $current_version → $new_version"
  fi

  if [[ "$OPT_DO_TAG" == "true" ]]; then
    if ! _is_prerelease "$new_version"; then
    local current_branch; current_branch="$(git rev-parse --abbrev-ref HEAD)"
    printf '%s\n' "${YELLOW}Next steps:${RESET}" >&2
          printf '%s\n' "  git push ${OPT_REMOTE} ${current_branch}" >&2
      printf '%s\n' "  git push ${OPT_REMOTE} ${OPT_TAG_PREFIX}${new_version}" >&2
    fi
  fi

  # Stdout only: computed version (for pipelines)
  printf '%s\n' "$new_version"
}

# --------------------------------- entry --------------------------------------
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  main "$@"
fi
