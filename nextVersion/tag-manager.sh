#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Tag manager for next-version
# Manages git tags for versioning

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C

# Source common utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# ---- trap unexpected errors with context ---------------------------------------------------------
err_trap() {
  local exit_code=$?
  local line=${1:-?}
  local cmd=${2:-?}
  printf 'Error (exit %d) at line %s: %s\n' "$exit_code" "$line" "$cmd" >&2
  exit "$exit_code"
}
trap 'err_trap "$LINENO" "$BASH_COMMAND"' ERR

# ---- guards --------------------------------------------------------------------------------------
(( BASH_VERSINFO[0] >= 4 )) || { echo "Bash ≥ 4 required" >&2; exit 1; }
command -v git >/dev/null 2>&1 || { echo "git not found in PATH" >&2; exit 1; }

# ---- program id ---------------------------------------------------------------------------------
readonly PROG="${0##*/}"

# ---- configurables (env overrides) ---------------------------------------------------------------
REMOTE="${REMOTE:-origin}"
TAG_GLOB="${TAG_GLOB:-v[0-9]*.[0-9]*.[0-9]*}"
TAG_SIGN="${TAG_SIGN:-0}"
ASSUME_YES="${ASSUME_YES:-0}"
PUSH_AFTER_CREATE="${PUSH_AFTER_CREATE:-0}"
ALLOW_DIRTY_TAG="${ALLOW_DIRTY_TAG:-0}"
DRY_RUN="${DRY_RUN:-0}"
LOCAL_ONLY="${LOCAL_ONLY:-0}"
REMOTE_ONLY="${REMOTE_ONLY:-0}"
PROTECT_GLOB="${PROTECT_GLOB:-}"
FIRST_PARENT="${FIRST_PARENT:-0}"
FETCH_BEFORE_CLEANUP="${FETCH_BEFORE_CLEANUP:-1}"
PROTECT_CURRENT="${PROTECT_CURRENT:-1}"
TAG_MSG_PREFIX="${TAG_MSG_PREFIX:-next-version}"

# ---- small log helpers --------------------------------------------------------------------------
# die(), warn(), info() functions are now sourced from version-utils.sh
is_tty(){ [[ -t 0 && -t 1 ]]; }

# is_true() function is now sourced from version-utils.sh

confirm() {
  local prompt="${1:-Proceed?}"
  if is_true "$ASSUME_YES"; then return 0; fi
  if ! is_tty; then
    warn "Non-interactive session; confirmation required. Set ASSUME_YES=1 to proceed automatically."
    return 1
  fi
  read -r -p "$prompt (y/N): " reply
  shopt -s nocasematch
  [[ "$reply" =~ ^y(es)?$ ]]
}

# dry-run aware executor
run_cmd() {
  if is_true "$DRY_RUN"; then
    printf '[DRY-RUN] ' >&2
    printf '%q ' "$@" >&2
    printf '\n' >&2
    return 0
  fi
  "$@"
}

ensure_git_repo()       { git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "Not inside a git repository."; }
ensure_remote_exists()  { git remote get-url "$REMOTE" >/dev/null 2>&1 || die "Remote '$REMOTE' not found."; }

print_effective_settings() {
  cat <<EOS
Current effective settings:
  REMOTE=$REMOTE
  TAG_GLOB=$TAG_GLOB
  TAG_SIGN=$TAG_SIGN
  ASSUME_YES=$ASSUME_YES
  PUSH_AFTER_CREATE=$PUSH_AFTER_CREATE
  ALLOW_DIRTY_TAG=$ALLOW_DIRTY_TAG
  DRY_RUN=$DRY_RUN
  LOCAL_ONLY=$LOCAL_ONLY
  REMOTE_ONLY=$REMOTE_ONLY
  PROTECT_GLOB=${PROTECT_GLOB:-"(none)"}
  FIRST_PARENT=$FIRST_PARENT
  FETCH_BEFORE_CLEANUP=$FETCH_BEFORE_CLEANUP
  PROTECT_CURRENT=$PROTECT_CURRENT
  TAG_MSG_PREFIX=$TAG_MSG_PREFIX
EOS
}

# ---- UI -----------------------------------------------------------------------------------------
show_help() {
  cat <<EOF
Tag Manager for next-version

Usage: $PROG [command] [options]

Commands:
  list [glob]                List tags (sorted by version). Default glob: ${TAG_GLOB}
  cleanup [keep] [glob]      Delete old release tags (keep=10 default). Default glob: ${TAG_GLOB}
  create <version> [commit]  Create tag v<version> (accepts "1.2.3" or "v1.2.3") at [commit] (default: HEAD)
  info <tag>                 Show details and changes since previous release tag

Environment variables:
  REMOTE, TAG_GLOB, TAG_SIGN, ASSUME_YES, PUSH_AFTER_CREATE, ALLOW_DIRTY_TAG,
  DRY_RUN, LOCAL_ONLY, REMOTE_ONLY, PROTECT_GLOB, FIRST_PARENT,
  FETCH_BEFORE_CLEANUP, PROTECT_CURRENT, TAG_MSG_PREFIX

Examples:
  $PROG list
  $PROG list 'v2.*.*'
  $PROG cleanup 5
  $PROG cleanup 20 'v1.*.*'
  $PROG create 1.2.0
  $PROG create v1.2.1 3f2c1d2
  $PROG info v1.1.2

EOF
  print_effective_settings
}

# ---- helpers -------------------------------------------------------------------------------------
prev_tag_for() {
  # Return the next *older* tag (version sort) relative to \$1 within TAG_GLOB
  local target="$1"
  git tag -l "$TAG_GLOB" --sort=-version:refname \
    | awk -v t="$target" '$0==t {getline; if ($0!="") print $0; exit}'
}

normalize_version_to_tag() {
  # Accept "1.2.3" or "v1.2.3" -> "v1.2.3"; validate strict x.y.z
  local v="$1"
  if [[ "$v" =~ ^v([0-9]+\.[0-9]+\.[0-9]+)$ ]]; then
    printf '%s\n' "v${BASH_REMATCH[1]}"
  elif [[ "$v" =~ ^([0-9]+\.[0-9]+\.[0-9]+)$ ]]; then
    printf 'v%s\n' "${BASH_REMATCH[1]}"
  else
    die "Invalid version '$v'. Expected x.y.z or vx.y.z"
  fi
}

# ---- commands ------------------------------------------------------------------------------------
list_tags() {
  ensure_git_repo
  local glob="${1:-$TAG_GLOB}"

  info "=== Tags (sorted by version) for glob: ${glob} ==="
  if ! git tag -l "$glob" | grep -q .; then
    info "(no tags match)"
    info "Total tags: 0"
    return 0
  fi
  git -c pager.tag=false tag -l "$glob" --sort=-version:refname | nl -ba
  echo
  local count
  count=$(git tag -l "$glob" | sed -n '$=' || true)
  printf "Total tags: %s\n" "${count:-0}"
}

cleanup_tags() {
  ensure_git_repo

  local keep_count="${1:-10}"
  local glob="${2:-$TAG_GLOB}"

  [[ "$keep_count" =~ ^[0-9]+$ ]] || die "keep must be a non-negative integer."
  if is_true "$REMOTE_ONLY" && is_true "$LOCAL_ONLY"; then
    die "REMOTE_ONLY and LOCAL_ONLY are mutually exclusive."
  fi

  local do_local=1 do_remote=1
  is_true "$REMOTE_ONLY" && do_local=0
  is_true "$LOCAL_ONLY"  && do_remote=0

  if (( do_remote )); then
    ensure_remote_exists
    if is_true "$FETCH_BEFORE_CLEANUP" && ! is_true "$DRY_RUN"; then
      info "Fetching tags from ${REMOTE}..."
      run_cmd git fetch --tags --prune "$REMOTE"
    fi
  fi

  info "=== Tag Cleanup ==="
  info "Glob pattern: ${glob}"
  [[ -n "$PROTECT_GLOB" ]] && info "Protected patterns: ${PROTECT_GLOB}"
  info "Keep newest: ${keep_count}"
  info "Local deletions: $([[ $do_local -eq 1 ]] && echo enabled || echo disabled)"
  info "Remote deletions on ${REMOTE}: $([[ $do_remote -eq 1 ]] && echo enabled || echo disabled)"
  info "DRY_RUN: $([[ $(is_true "$DRY_RUN"; echo $?) -eq 0 ]] && echo yes || echo no)"

  mapfile -t tags < <(git tag -l "$glob" --sort=-version:refname)
  local total_tags="${#tags[@]}"
  printf "Found %d matching tags.\n" "$total_tags"

  if (( total_tags <= keep_count )); then
    info "No cleanup needed."
    return 0
  fi

  # tags to delete = all after first keep_count
  local -a to_delete=( "${tags[@]:$keep_count}" )

  # Build protection set: PROTECT_GLOB (as globs) + tags pointing at HEAD (names)
  local -a protect_pats=()
  if [[ -n "$PROTECT_GLOB" ]]; then
    # shellcheck disable=SC2206  # intended word-splitting on space-separated globs
    protect_pats=( $PROTECT_GLOB )
  fi
  if is_true "$PROTECT_CURRENT"; then
    while IFS= read -r t; do
      [[ -n "$t" ]] && protect_pats+=( "$t" )
    done < <(git tag --points-at HEAD || true)
  fi

  # Apply protection
  if (( ${#protect_pats[@]} > 0 )); then
    local -a filtered=()
    local tag pat protected
    for tag in "${to_delete[@]}"; do
      protected=0
      for pat in "${protect_pats[@]}"; do
        # [[ str == glob ]] performs glob matching; this is desired
        if [[ "$tag" == "$pat" ]]; then
          protected=1; break
        fi
      done
      (( protected == 0 )) && filtered+=( "$tag" )
    done
    to_delete=( "${filtered[@]}" )
  fi

  if (( ${#to_delete[@]} == 0 )); then
    info "After protection filters, nothing to delete."
    return 0
  fi

  info "Tags to delete (${#to_delete[@]}):"
  printf '%s\n' "${to_delete[@]}"
  echo

  local scope_msg
  if   (( do_local )) && (( do_remote )); then scope_msg="locally and on '${REMOTE}'"
  elif (( do_local )); then                      scope_msg="locally"
  else                                            scope_msg="on '${REMOTE}'"
  fi

  if confirm "Delete the above tags ${scope_msg}?"; then
    info "Deleting..."
    local tag
    for tag in "${to_delete[@]}"; do
      [[ -n "$tag" ]] || continue
      if (( do_local )); then
        info "  - local:  $tag"
        run_cmd git tag -d -- "$tag" >/dev/null
      fi
      if (( do_remote )); then
        info "  - remote: $tag"
        # Try modern syntax first; fallback to refspec form
        run_cmd git push --delete "$REMOTE" "$tag" || run_cmd git push "$REMOTE" ":refs/tags/$tag"
      fi
    done
    info "Tag cleanup completed."
  else
    info "Cancelled."
  fi
}

create_tag() {
  ensure_git_repo

  local ver_input="${1:-}"
  local commitish="${2:-HEAD}"
  [[ -n "$ver_input" ]] || die "Usage: $PROG create <version> [commit]"

  local tag_name
  tag_name=$(normalize_version_to_tag "$ver_input") || exit 1

  # Resolve commit
  local commit
  commit="$(git rev-parse --verify --quiet "${commitish}^{commit}")" \
    || die "Commit '${commitish}' is not a valid commit."

  # Already exists?
  if git tag -l -- "$tag_name" | grep -qx -- "$tag_name"; then
    die "Tag ${tag_name} already exists."
  fi

  # Signed tag prerequisites
  if is_true "$TAG_SIGN"; then
    command -v gpg >/dev/null 2>&1 || die "TAG_SIGN=1 but gpg not found."
    git config --get user.signingkey >/dev/null || die "TAG_SIGN=1 but git user.signingkey is not configured."
  fi

  # Dirty tree warning only when tagging HEAD and not allowed
  if ! is_true "$ALLOW_DIRTY_TAG" && [[ "$commit" == "$(git rev-parse HEAD)" ]]; then
    if [[ -n "$(git status --porcelain=v1 --untracked-files=normal 2>/dev/null)" ]]; then
      warn "You have uncommitted and/or untracked changes."
      confirm "Continue creating ${tag_name} at ${commit:0:7}?" || die "Aborted."
    fi
  fi

  # Warn if another tag already points to the same commit (helps avoid duplicates)
  local same_commit_tags
  same_commit_tags="$(git tag --points-at "$commit" || true)"
  if [[ -n "$same_commit_tags" ]]; then
    warn "Other tag(s) already point to this commit:"
    printf '  %s\n' "$same_commit_tags"
  fi

  local msg="${TAG_MSG_PREFIX} ${tag_name}"
  info "Creating tag: ${tag_name} at ${commit}"

  if is_true "$TAG_SIGN"; then
    run_cmd git tag -s -m "$msg" -- "$tag_name" "$commit"
  else
    run_cmd git tag -a -m "$msg" -- "$tag_name" "$commit"
  fi
  info "Tag created locally."  # in dry-run, only simulated

  if is_true "$PUSH_AFTER_CREATE"; then
    ensure_remote_exists
    info "Pushing ${tag_name} to ${REMOTE}..."
    run_cmd git push "$REMOTE" "$tag_name"
    info "Tag pushed."
  else
    info "Push with:"
    info "  git push ${REMOTE} ${tag_name}"
  fi
}

show_tag_info() {
  ensure_git_repo

  local tag="${1:-}"
  [[ -n "$tag" ]] || die "Usage: $PROG info <tag>"

  git tag -l -- "$tag" | grep -qx -- "$tag" || die "Tag ${tag} does not exist."

  local commit
  commit="$(git rev-parse --verify "${tag}^{commit}")" || die "Could not resolve ${tag} to a commit"

  info "=== Tag Information: ${tag} ==="

  local tag_type
  tag_type="$(git cat-file -t "$tag" 2>/dev/null || true)"
  if [[ "$tag_type" == "tag" ]]; then
    local tagger_line tag_subject
    tagger_line="$(git for-each-ref --format='%(taggername) <%(taggeremail)> | %(taggerdate:iso8601)' "refs/tags/$tag" || true)"
    tag_subject="$(git for-each-ref --format='%(contents:subject)' "refs/tags/$tag" || true)"
    info "Tag type : annotated"
    [[ -n "$tagger_line"  ]] && info "Tagger   : ${tagger_line}"
    [[ -n "$tag_subject"  ]] && info "Message  : ${tag_subject}"
  else
    info "Tag type : lightweight"
  fi

  info "Commit  : ${commit}"
  info "Date    : $(git --no-pager log -1 --format=%cd --date=iso-strict "${commit}")"
  info "Author  : $(git --no-pager log -1 --format='%an <%ae>' "${commit}")"
  info "Subject : $(git --no-pager log -1 --format=%s "${commit}")"
  echo

  info "Changes since previous release tag (${TAG_GLOB}):"
  local prev_tag
  prev_tag="$(prev_tag_for "${tag}")"
  if [[ -n "$prev_tag" ]]; then
    info "(prev: ${prev_tag})"
    local -a LOG_OPTS=(--oneline --no-decorate)
    is_true "$FIRST_PARENT" && LOG_OPTS+=(--first-parent)
    git --no-pager log "${LOG_OPTS[@]}" "${prev_tag}..${tag}"
    echo
    local -a COUNT_OPTS=()
    is_true "$FIRST_PARENT" && COUNT_OPTS+=(--first-parent)
    info "Commit count: $(git rev-list --count "${COUNT_OPTS[@]}" "${prev_tag}..${tag}")"
    info "Diffstat:"
    git --no-pager diff --stat "${prev_tag}..${tag}" || true
  else
    info "No previous matching release tag found."
  fi
}

# ---- entry point --------------------------------------------------------------------------------
case "${1:-help}" in
  list)    shift; list_tags "${1:-}";;
  cleanup) shift; cleanup_tags "${1:-10}" "${2:-}";;
  create)  shift; create_tag "${1:-}" "${2:-HEAD}";;
  info)    shift; show_tag_info "${1:-}";;
  help|-h|--help) show_help;;
  *) echo "Unknown command: ${1:-}"; echo; show_help; exit 1;;
esac
