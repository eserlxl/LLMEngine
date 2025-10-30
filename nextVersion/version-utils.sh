#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Version utilities for next-version
# Common functions used across version management scripts

set -Eeuo pipefail
IFS=$'\n\t'
umask 022
export LC_ALL=C

# ------------------- error & messaging -------------------
# Print to stderr to keep stdout clean for machine output.
_die()   { printf '%s\n' "${RED:-}Error:${RESET:-} $*">&2; exit 1; }
_warn()  { printf '%s\n' "${YELLOW:-}Warning:${RESET:-} $*">&2; }
_info()  { printf '%s\n' "${CYAN:-}$*${RESET:-}">&2; }
_ok()    { printf '%s\n' "${GREEN:-}$*${RESET:-}">&2; }

# Export functions for use by other scripts
die()    { _die "$@"; }
warn()   { _warn "$@"; }
info()   { _info "$@"; }
ok()     { _ok "$@"; }

# ------------------- better ERR diagnostics -------------------
_stacktrace() {
  local i
  _warn "Stack (most recent call first):"
  for (( i=0; i<${#FUNCNAME[@]}-1; i++ )); do
    local func="${FUNCNAME[$i]:-main}"
    local line="${BASH_LINENO[$i-1]:-?}"
    local src="${BASH_SOURCE[$i]:-?}"
    _warn "  at ${func} (${src}:${line})"
  done
}

# shellcheck disable=SC2154
trap '_status=$?; [[ $_status -ne 0 ]] && _warn "Command failed: ${BASH_COMMAND} (exit $_status)"; _stacktrace; exit $_status' ERR

# ------------------- color utilities -------------------
init_colors() {
  # honor NO_COLOR spec (https://no-color.org/)
  local no_color="${1:-${NO_COLOR:-false}}"
  if [[ "$no_color" != "true" && -t 2 ]]; then
    RED=$'\033[0;31m'
    GREEN=$'\033[0;32m'
    YELLOW=$'\033[1;33m'
    CYAN=$'\033[0;36m'
    RESET=$'\033[0m'
  else
    RED='' GREEN='' YELLOW='' CYAN='' RESET=''
  fi
}

# ------------------- command requirements -------------------
require_cmd() {
  local need=("$@")
  # Default baseline if none provided
  if ((${#need[@]} == 0)); then
    need=(git sed grep awk)
    # path helpers are handled by _realpath fallback
  fi
  local missing=()
  for c in "${need[@]}"; do
    command -v "$c" >/dev/null 2>&1 || missing+=("$c")
  done
  ((${#missing[@]}==0)) || _die "Missing required tools: ${missing[*]}"
}

# ------------------- boolean utilities -------------------
is_true() {
  # normalize various truthy/falsey inputs to 0/1 return code
  local v="${1:-}"
  case "${v,,}" in
    1|true|t|yes|y|on)  return 0 ;;
    0|false|f|no|n|off) return 1 ;;
    *)                  return 1 ;;
  esac
}

is_false() {
  ! is_true "${1:-}"
}

# ------------------- JSON utilities -------------------
json_escape() {
  # Comprehensive JSON string escaper
  local s="${1:-}"
  s="${s//\\/\\\\}"  # backslash
  s="${s//\"/\\\"}"  # double quote
  s="${s//$'\n'/\\n}"  # newline
  s="${s//$'\r'/\\r}"  # carriage return
  s="${s//$'\t'/\\t}"  # tab
  printf '%s' "$s"
}

# ------------------- portable realpath -------------------
_realpath() {
  # $1: path
  local p="$1"
  if command -v realpath >/dev/null 2>&1; then
    realpath -- "$p"
  elif command -v readlink >/dev/null 2>&1; then
    # GNU readlink -f works; on BSD we emulate via Python
    if readlink -f / >/dev/null 2>&1; then
      readlink -f -- "$p"
    elif command -v python3 >/dev/null 2>&1; then
      python3 - <<'PY' "$p"
import os,sys
print(os.path.realpath(sys.argv[1]))
PY
    else
      # Fallback: best effort (no symlink resolution)
      (cd -- "$(dirname -- "$p")" 2>/dev/null && printf '%s/%s\n' "$PWD" "$(basename -- "$p")") || printf '%s\n' "$p"
    fi
  else
    printf '%s\n' "$p"
  fi
}

# ------------------- path resolution -------------------
# Exports: PROJECT_ROOT, VERSION_FILE
resolve_script_paths() {
  local script_path="$1"
  local repo_root="${2:-}"

  local script_dir project_root version_file try_git_root
  script_dir="$(dirname -- "$(_realpath "$script_path")")"

  if [[ -n "$repo_root" ]]; then
    [[ -d "$repo_root" ]] || _die "Repository root '$repo_root' does not exist"
    project_root="$(_realpath "$repo_root")"
  else
    if command -v git >/dev/null 2>&1; then
      try_git_root="$(git -C "$script_dir" rev-parse --show-toplevel 2>/dev/null || true)"
      if [[ -n "$try_git_root" && -d "$try_git_root" ]]; then
        project_root="$try_git_root"
      else
        project_root="$(dirname -- "$script_dir")"
      fi
    else
      project_root="$(dirname -- "$script_dir")"
    fi
  fi

  version_file="$project_root/VERSION"
  cd "$project_root" || _die "Cannot cd to $project_root"

  export PROJECT_ROOT="$project_root"
  export VERSION_FILE="$version_file"
}

# ------------------- cleanup registry -------------------
# Register any temp file path; on exit they will be removed.
declare -a _TMP_FILES=()
register_tmp() { [[ -n "${1:-}" ]] && _TMP_FILES+=("$1"); }
_cleanup_tmp() {
  local f
  for f in "${_TMP_FILES[@]:-}"; do
    if [[ -n "$f" && -e "$f" ]]; then
      rm -f -- "$f"
    fi
  done
}
trap '_cleanup_tmp' EXIT INT TERM

# ------------------- file operations -------------------
# Atomic write: write to temp in same directory, then mv.
safe_write_file() {
  local target_file="$1"; shift
  local content="${1-}"
  local dir base tmp

  dir="$(dirname -- "$target_file")"
  base="$(basename -- "$target_file")"

  # Portable mktemp (BSD/GNU)
  tmp="$(mktemp "$dir/.${base}.XXXXXX" 2>/dev/null || mktemp 2>/dev/null)" || _die "mktemp failed for $target_file"
  register_tmp "$tmp"

  printf '%s\n' "$content" > "$tmp" || _die "Cannot write temp file"

  # Best-effort durability without relying on non-portable `sync -f`
  if command -v python3 >/dev/null 2>&1; then
    python3 - <<'PY' "$tmp"
import os, sys
p=sys.argv[1]
fd=os.open(p, os.O_RDONLY)
try:
    os.fsync(fd)
finally:
    os.close(fd)
PY
  fi

  mv -f -- "$tmp" "$target_file" || _die "Cannot move temp file into place"
}

# ------------------- hashing -------------------
# Prefer sha256; fall back gracefully.
_hash_file() {
  local f="$1"
  [[ -f "$f" ]] || { _warn "hash: file not found: $f"; printf ''; return 1; }
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum -- "$f" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 -- "$f" | awk '{print $1}'
  elif command -v openssl >/dev/null 2>&1; then
    # openssl writes "SHA256(file)= hash" to stdout
    openssl dgst -sha256 -- "$f" | awk '{print $NF}'
  elif command -v sha1sum >/dev/null 2>&1; then
    sha1sum -- "$f" | awk '{print $1}'
  elif command -v md5sum >/dev/null 2>&1; then
    md5sum -- "$f" | awk '{print $1}'
  else
    _warn "No hashing tool found"; printf ''
  fi
}

# ------------------- git utilities -------------------
check_git_repo() {
  git rev-parse --git-dir >/dev/null 2>&1 || _die "Not in a git repository"
}

check_git_branch() {
  git symbolic-ref -q HEAD >/dev/null || _die "Detached HEAD; checkout a branch before committing/tagging"
}

check_git_identity() {
  local n e
  n="$(git config --get user.name || true)"
  e="$(git config --get user.email || true)"
  [[ -n "$n" ]] || _warn "git user.name is not set"
  [[ -n "$e" ]] || _warn "git user.email is not set"
}

# Validate git reference
verify_ref() {
  local ref="$1"
  git -c color.ui=false rev-parse -q --verify "${ref}^{commit}" >/dev/null || _die "Invalid reference: $ref"
}

# ------------------- version file utilities -------------------
read_version_file() {
  local vf="$1"
  [[ -f "$vf" ]] || { printf ''; return 0; }
  local version
  version=$(tr -d '[:space:]' < "$vf" 2>/dev/null || printf '')
  printf '%s' "$version"
}

validate_version_file_path() {
  local version_file="$1"
  local project_root="$2"
  local resolved
  resolved="$(_realpath "$version_file" 2>/dev/null || true)"
  [[ -n "$resolved" ]] || _die "VERSION path is a broken symlink"
  case "$resolved" in
    "$project_root"/*) : ;;
    *) _die "VERSION resolves outside repo" ;;
  esac
}

# ------------------- tag utilities -------------------
# Escape only glob specials for `git tag --list` (fnmatch), not regex.
sanitize_tag_prefix() {
  local p="$1"
  p="${p//\\/\\\\}"  # \ -> \\
  p="${p//\*/\\*}"   # * -> \*
  p="${p//\?/\\?}"   # ? -> \?
  p="${p//\[/\\[}"   # [ -> \[
  p="${p//\]/\\]}"   # ] -> \]
  printf '%s' "$p"
}

last_tag_for_prefix() {
  local tag_prefix="$1"
  local sanitized pattern t=""
  sanitized="$(sanitize_tag_prefix "$tag_prefix")"
  pattern="${sanitized}[0-9]*.[0-9]*.[0-9]*"
  t="$(git tag --list "$pattern" --sort=-version:refname | head -n1 || true)"
  [[ -n "$t" ]] || t="$(git tag --list "$pattern" --sort=-v:refname | head -n1 || true)"
  printf '%s' "$t"
}

# ------------------- validation & semver -------------------
is_uint() { [[ "$1" =~ ^[0-9]+$ ]]; }

# Public: strict X.Y.Z (no pre/build)
is_semver() { _is_semver_core "$@"; }

# Split semver into components (echo "major minor patch")
split_semver() {
  local version="$1" major minor patch
  IFS='.' read -r major minor patch <<< "$version"
  printf '%s\n%s\n%s' "$major" "$minor" "$patch"
}

# Strict semver core X.Y.Z (no leading zeros)
_is_semver_core() {
  [[ "$1" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]
}

# Semver with optional -prerelease
_is_semver_with_prerelease() {
  [[ "$1" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?$ ]]
}

# Semver with optional -prerelease and +build metadata
_is_semver_full() {
  [[ "$1" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?(\+[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?$ ]]
}

semver_normalize() {
  # Prints normalized X.Y.Z or errors out
  local v="$1"
  _is_semver_core "$v" || _die "Not a core semver: $v"
  # Strip leading zeros by arithmetic context
  local M m p
  IFS='.' read -r M m p <<< "$v"
  printf '%d.%d.%d\n' "$M" "$m" "$p"
}

# Compare two strict X.Y.Z versions
# returns: 0 if equal, 1 if v1>v2, 2 if v1<v2
semver_cmp() {
  local v1 v2 M1 m1 p1 M2 m2 p2
  v1="$(semver_normalize "$1")"
  v2="$(semver_normalize "$2")"
  IFS='.' read -r M1 m1 p1 <<< "$v1"
  IFS='.' read -r M2 m2 p2 <<< "$v2"
  if   (( M1 > M2 )); then return 1
  elif (( M1 < M2 )); then return 2
  elif (( m1 > m2 )); then return 1
  elif (( m1 < m2 )); then return 2
  elif (( p1 > p2 )); then return 1
  elif (( p1 < p2 )); then return 2
  else return 0
  fi
}

validate_version_format() {
  local version="$1"
  local allow_prerelease="${2:-false}"
  local allow_build="${3:-false}"

  if [[ "$allow_build" == "true" ]]; then
    _is_semver_full "$version" && return 0
    printf "%s\n" "${YELLOW}Expected: MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD] (e.g., 1.2.3-rc.1+001)${RESET}" >&2
    _die "Invalid version format: $version"
  elif [[ "$allow_prerelease" == "true" ]]; then
    _is_semver_with_prerelease "$version" && return 0
    printf "%s\n" "${YELLOW}Expected: MAJOR.MINOR.PATCH or MAJOR.MINOR.PATCH-PRERELEASE (e.g., 1.2.3 or 1.2.3-rc.1)${RESET}" >&2
    _die "Invalid version format: $version"
  else
    _is_semver_core "$version" && return 0
    printf "%s\n" "${YELLOW}Expected: MAJOR.MINOR.PATCH (e.g., 1.2.3)${RESET}" >&2
    printf "%s\n" "${YELLOW}Note: Pre-releases require enablement; build metadata is not allowed${RESET}" >&2
    _die "Invalid version format: $version"
  fi
}

# ------------------- standalone CLI -------------------
_usage() {
  cat <<EOF
Usage: $(basename "$0") <command> [args...]

Commands:
  last-tag <tag_prefix>              Get last tag for a prefix (e.g., "v")
  hash-file <file_path>              Print file hash (sha256 preferred)
  read-version <version_file>        Read version, stripped of whitespace
  validate-version <ver> [pre] [build]  Validate semver; set 'pre' or 'build' to true
  semver-cmp <v1> <v2>              Exit code 0(equal), 10(v1>v2), 11(v1<v2)

Env:
  NO_COLOR=true                      Disable ANSI colors

Examples:
  $(basename "$0") last-tag v
  $(basename "$0") hash-file CMakeLists.txt
  $(basename "$0") read-version VERSION
  $(basename "$0") validate-version 1.2.3
  $(basename "$0") semver-cmp 1.2.3 1.4.0
EOF
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  init_colors "${NO_COLOR:-false}"
  case "${1:-}" in
    last-tag)
      shift; [[ $# -ge 1 ]] || _die "last-tag requires <tag_prefix>"; last_tag_for_prefix "$1" ;;
    hash-file)
      shift; [[ $# -ge 1 ]] || _die "hash-file requires <file_path>"; _hash_file "$1" ;;
    read-version)
      shift; [[ $# -ge 1 ]] || _die "read-version requires <version_file>"; read_version_file "$1" ;;
    validate-version)
      shift; [[ $# -ge 1 ]] || _die "validate-version requires <version>"; validate_version_format "${1:?}" "${2:-false}" "${3:-false}" ;;
    semver-cmp)
      shift; [[ $# -ge 2 ]] || _die "semver-cmp requires <v1> <v2>"; if semver_cmp "$1" "$2"; then exit 0; else rc=$?; [[ $rc -eq 1 ]] && exit 10 || exit 11; fi ;;
    -h|--help|'') _usage; [[ -n "${1:-}" ]] || exit 0 ;;
    *) _usage; exit 1 ;;
  esac
fi
