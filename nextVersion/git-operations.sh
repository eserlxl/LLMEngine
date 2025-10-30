#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# Git operations for next-version
# Handles git commits, tags, and push operations

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C
# Avoid pagers or extra locking in CI/automation contexts.
export GIT_PAGER=cat PAGER=cat GIT_OPTIONAL_LOCKS=0

# Source utilities
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/version-utils.sh"

# Initialize colors
init_colors

# Fail loudly with context
# shellcheck disable=SC2154
trap '{
  local ec=$?; local cmd=${BASH_COMMAND:-}
  printf "%s\n" "${RED:-}Error:${RESET:-} line $LINENO: \`$cmd\` (exit $ec)" >&2
  exit $ec
}' ERR

# --- Helper functions --------------------------------------------------------
to_bool() {
    # normalize various truthy/falsey inputs to 0/1 return code
    local v="${1:-}"
    case "${v,,}" in
        1|true|t|yes|y|on)  return 0 ;;
        0|false|f|no|n|off) return 1 ;;
        *)                  return 1 ;;
    esac
}

# is_true() and is_false() functions are now sourced from version-utils.sh

git_in_repo() {
    git rev-parse --git-dir >/dev/null 2>&1
}

is_detached_head() {
    ! git symbolic-ref -q HEAD >/dev/null
}

has_commit_history() {
    git rev-parse --verify -q HEAD >/dev/null 2>&1
}

current_branch_name() {
    # safe even when upstream missing; caller should have checked detached HEAD
    git rev-parse --abbrev-ref HEAD 2>/dev/null
}

# require_git() function is now replaced with check_git_repo() from version-utils.sh

# --- Git state checks --------------------------------------------------------
check_dirty_tree() {
    local exclude_paths=()
    # Always exclude VERSION from dirty check since we'll be updating it
    exclude_paths+=("VERSION")
    if is_false "${UPDATE_CMAKE:-true}"; then
        exclude_paths+=("CMakeLists.txt")
    fi

    # Fast pre-check (untracked excluded later)
    if ! git status --porcelain=v1 >/dev/null 2>&1; then
        die "Not in a git repository"
    fi

    local diff_args=(--no-ext-diff -- .)
    for path in "${exclude_paths[@]}"; do
        diff_args+=(":(exclude)$path")
    done

    if ! git diff --quiet "${diff_args[@]}"; then
        local allowed="VERSION"
        is_false "${UPDATE_CMAKE:-true}" && allowed+=" and CMakeLists.txt"

        printf '%s' "${RED}Error:${RESET} working tree has disallowed changes. Use --allow-dirty to override." >&2
        [[ -n "$allowed" ]] && printf ' Allowed: %s.' "$allowed" >&2
        printf '\nDirty files (excludes applied):\n' >&2
        git diff --name-only "${diff_args[@]}" >&2
        exit 1
    fi

    # Warn on untracked files (not fatal)
    if git ls-files --others --exclude-standard | grep -q .; then
        printf '%s\n' "${YELLOW}Warning:${RESET} untracked files present (ignored)." >&2
    fi
}

check_git_prerequisites() {
    local do_commit="$1"
    local do_tag="$2"
    local do_push="$3"
    local push_tags="$4"
    
    check_git_repo
    git_in_repo || die "Not in a git repository"
    
    # Check for detached HEAD if committing/tagging/pushing
    if is_true "$do_commit" || is_true "$do_tag" || is_true "$do_push" || is_true "$push_tags"; then
        if is_detached_head; then
            die "Detached HEAD; checkout a branch before continuing (e.g., git switch <branch>)"
        fi
    fi
    
    # Tagging/pushing requires at least one commit present
    if is_true "$do_tag" || is_true "$do_push" || is_true "$push_tags"; then
        has_commit_history || die "Repository has no commits yet"
    fi
}

check_signing_prerequisites() {
    local signed_tag="$1"
    local commit_sign="$2"
    local have_key=0
    git config --get user.signingkey >/dev/null 2>&1 && have_key=1
    
    if is_true "$signed_tag" && (( ! have_key )); then
        warn "--signed-tag requested but no user.signingkey configured"
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            die "--signed-tag requested in CI but no signing key configured"
        fi
    fi
    
    if is_true "$commit_sign" && (( ! have_key )); then
        warn "--sign-commit requested but no user.signingkey configured"
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            die "--sign-commit requested in CI but no signing key configured"
        fi
    fi
    
    if is_true "$signed_tag" || is_true "$commit_sign"; then
        [[ -t 1 && -z "${GPG_TTY:-}" ]] && printf '%s\n' "${YELLOW}Hint:${RESET} export GPG_TTY=\$(tty) may be required for pinentry." >&2
        command -v gpg >/dev/null 2>&1 || warn "gpg not found; signing may fail"
    fi
    
    # If user has commit.gpgSign=true globally but we don't want signing, warn
    if is_false "$commit_sign" && git config --get commit.gpgSign >/dev/null 2>&1; then
        printf '%s\n' "${YELLOW}Note:${RESET} commit.gpgSign=true is configured; Git may sign commits anyway. Forcing --no-gpg-sign." >&2
    fi
}

# --- File staging ------------------------------------------------------------
stage_files() {
    local version_file="$1"
    local update_cmake="$2"
    local new_version="$3"
    local project_root="$4"
    
    # Only stage VERSION if it's not a pre-release
    if [[ "$new_version" != *-* && -f "$version_file" ]]; then
        git add -- "$version_file"
    fi
    
    # Handle CMakeLists.txt staging
    if is_true "$update_cmake"; then
        [[ -f "$project_root/CMakeLists.txt" ]] && git add -- "$project_root/CMakeLists.txt"
    else
        # Ensure it's not accidentally staged unless ALLOW_DIRTY is true
        if is_false "${ALLOW_DIRTY:-false}"; then
            git reset -- "$project_root/CMakeLists.txt" 2>/dev/null || true
        fi
    fi
}

# --- Commit operations -------------------------------------------------------
create_commit() {
    local version_file="$1"
    local update_cmake="$2"
    local new_version="$3"
    local current_version="$4"
    local commit_msg="$5"
    local no_verify="$6"
    local commit_sign="$7"
    local tag_prefix="$8"
    local project_root="$9"
    local original_project_root="${10}"
    
    stage_files "$version_file" "$update_cmake" "$new_version" "$project_root"
    
    if git diff --cached --quiet; then
        if [[ "$new_version" == *-* ]]; then
            warn "Skipping commit: pre-release made no file changes"
        else
            warn "No staged changes to commit"
        fi
        return 0
    fi
    
    check_git_identity
    
    local commit_args=()
    is_true "$no_verify" && commit_args+=(--no-verify)
    
    if is_true "$commit_sign"; then
        commit_args+=(-S)
    else
        # If user has commit.gpgSign=true globally, force disable if we don't want it
        commit_args+=(--no-gpg-sign)
    fi
    
    if [[ -z "$commit_msg" ]]; then
        # Auto message
        local title="chore(release): ${tag_prefix}${new_version}${GITHUB_ACTIONS:+ [skip ci]}"
        commit_args+=(-m "$title")
        
        if [[ "$current_version" == "none" ]]; then
            commit_args+=(-m "bump: initial version ${new_version}")
        else
            commit_args+=(-m "bump: ${current_version} → ${new_version}")
        fi
        
        if [[ -n "${ANALYSIS_MESSAGE:-}" ]]; then
            commit_args+=(-m "$ANALYSIS_MESSAGE")
        fi
        
        # Add explanation if semantic analyzer is available and we're in CI
        if [[ -n "${GITHUB_ACTIONS:-}" && -x "$original_project_root/bin/semantic-version-analyzer.sh" ]]; then
            local explanation=""
            local analyzer_output
            analyzer_output="$("$original_project_root/bin/semantic-version-analyzer.sh" --verbose 2>/dev/null || true)"
            
            if [[ -n "$analyzer_output" ]]; then
                # Extract reason from verbose output
                local reason_line
                reason_line="$(printf '%s\n' "$analyzer_output" | grep -E '^Reason:' | head -1 || true)"
                if [[ -n "$reason_line" ]]; then
                    explanation="${reason_line#Reason: }"
                fi
            fi
            
            # If no explanation from analyzer, create a basic one
            if [[ -z "$explanation" ]]; then
                case "${BUMP_TYPE:-}" in
                    major) explanation="Major version bump due to significant changes" ;;
                    minor) explanation="Minor version bump due to new features or additions" ;;
                    patch) explanation="Patch version bump due to bug fixes or improvements" ;;
                    *) explanation="Version bump triggered by automated analysis" ;;
                esac
            fi
            
            if [[ -n "$explanation" ]]; then
                commit_args+=(-m "Reason: $explanation")
            fi
        fi
    fi
    
    # Limit commit to staged files if we know them; otherwise commit everything staged
    if [[ -z "$commit_msg" ]]; then
        git commit "${commit_args[@]}"
    else
        git commit "${commit_args[@]}" -m "$commit_msg"
    fi
    
    ok "Created commit"
    local commit_sha
    commit_sha="$(git rev-parse --short HEAD 2>/dev/null || echo "?")"
    info "Commit SHA: $commit_sha"
}

# --- Tag operations ----------------------------------------------------------
create_tag() {
    local new_version="$1"
    local tag_prefix="$2"
    local annotated_tag="$3"
    local signed_tag="$4"
    
    local tag_name="${tag_prefix}${new_version}"
    
    if [[ "$new_version" == *-* ]]; then
        printf '%s\n' "${YELLOW}Pre-release versions should not be tagged${RESET}" >&2
        die "Cannot create tag for pre-release version"
    fi
    
    if git rev-parse -q --verify "$tag_name" >/dev/null; then
        if [[ "$(git rev-parse "$tag_name^{commit}")" != "$(git rev-parse HEAD)" ]]; then
            die "tag $tag_name exists but not on HEAD"
        fi
        warn "Tag $tag_name already exists"
        return 0
    fi
    
    if is_true "$signed_tag"; then
        git tag -s "$tag_name" -m "Release ${tag_prefix}${new_version}"
        ok "Created signed tag: $tag_name"
    elif is_true "$annotated_tag"; then
        git tag -a "$tag_name" -m "Release ${tag_prefix}${new_version}"
        ok "Created annotated tag: $tag_name"
    else
        git tag "$tag_name"
        ok "Created lightweight tag: $tag_name"
    fi
    
    local tag_sha
    tag_sha="$(git rev-parse --short "$tag_name" 2>/dev/null || echo "?")"
    info "Tag SHA: $tag_sha"
}

# --- Push operations ---------------------------------------------------------
push_changes() {
    local remote="$1"
    local do_tag="$2"
    local new_version="$3"
    local tag_prefix="$4"
    local tag_name="${tag_prefix}${new_version}"
    
    # Check remote exists
    git remote get-url "$remote" >/dev/null 2>&1 || die "No remote '$remote' configured"
    
    local branch
    branch="$(current_branch_name)"
    info "Pushing changes to '$remote'..."
    
    # Add -u if upstream not set yet
    local push_args=("$remote" "$branch")
    if ! git rev-parse --abbrev-ref --symbolic-full-name "@{u}" >/dev/null 2>&1; then
        push_args=("$remote" "-u" "$branch")
    fi
    
    if is_true "$do_tag"; then
        # Atomic push ensures both branch and tag are updated together or not at all
        git push --atomic "$remote" "$branch" "$tag_name" || die "Failed to push branch and tag atomically"
    else
        git push "${push_args[@]}" || die "Failed to push branch $branch"
    fi
    
    ok "Push completed"
}

push_all_tags() {
    local remote="$1"
    
    # Check remote exists
    git remote get-url "$remote" >/dev/null 2>&1 || die "No remote '$remote' configured"
    
    info "Pushing all tags..."
    git push "$remote" --tags || die "Failed to push tags"
    ok "Tags push completed"
}

# --- Version order validation -------------------------------------------------
_check_version_greater_fallback() {
    # Fallback using sort -V when external validator is absent.
    # Returns 0 if $1 > $2, else 1.
    local a="$1" b="$2"
    [[ "$a" == "$b" ]] && return 1
    if printf '%s\n' "$b" "$a" | sort -V | tail -1 | grep -qx -- "$a"; then
        return 0
    fi
    return 1
}

check_version_order() {
    local new_version="$1"
    local tag_prefix="$2"
    local allow_nonmonotonic="$3"
    
    if [[ "$new_version" == *-* ]]; then
        return 0
    fi
    
    local last_tag last_version
    last_tag="$("$SCRIPT_DIR/version-utils.sh" last-tag "$tag_prefix" 2>/dev/null || true)"
    [[ -z "$last_tag" ]] && return 0
    
    last_version="${last_tag:${#tag_prefix}}"
    is_semver "$last_version" || return 0

    local ok=1
    if [[ -x "$SCRIPT_DIR/version-validator" ]]; then
        "$SCRIPT_DIR/version-validator" is_version_greater "$new_version" "$last_version" >/dev/null 2>&1 && ok=0 || ok=1
    else
        _check_version_greater_fallback "$new_version" "$last_version" && ok=0 || ok=1
    fi

    if (( ok != 0 )); then
        warn "New version $new_version is not greater than last tag $last_tag"
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            if ! to_bool "$allow_nonmonotonic"; then
            printf '%s\n' "${YELLOW}Use --allow-nonmonotonic-tag to override${RESET}" >&2
            die "NEW_VERSION ($new_version) must be greater than last tag ($last_tag)"
            fi
        fi
    fi
}

# --- Summary generation ------------------------------------------------------
generate_summary() {
    local do_commit="$1"
    local do_tag="$2"
    local do_push="$3"
    local push_tags="$4"
    local new_version="$5"
    local tag_prefix="$6"
    local remote="${7:-}"
    
    if is_true "$do_commit" || is_true "$do_tag" || is_true "$do_push" || is_true "$push_tags"; then
        info "Summary:"
        if is_true "$do_commit"; then
            local sha branch
            sha="$(git rev-parse --short HEAD 2>/dev/null || echo "?")"
            branch="$(current_branch_name)"
            printf '  Branch: %s\n' "$branch" >&2
            printf '  Commit: %s\n' "$sha" >&2
        fi
        if is_true "$do_tag"; then
            local tag_sha
            tag_sha="$(git rev-parse --short "${tag_prefix}${new_version}" 2>/dev/null || echo "?")"
            printf '  Tag: %s/%s\n' "${tag_prefix}${new_version}" "$tag_sha" >&2
        fi
        if is_true "$do_push"; then
            printf '  Remote: %s\n' "${remote:-<none>}" >&2
            printf '  Pushed: yes (branch%s)\n' "$(is_true "$do_tag" && printf ' + tag')" >&2
        elif is_true "$push_tags"; then
            printf '  Remote: %s\n' "${remote:-<none>}" >&2
            printf '  Tags pushed: yes\n' >&2
        else
            printf '  Pushed: no\n' >&2
        fi
    fi
}

# --- Main git operations function --------------------------------------------
perform_git_operations() {
    local version_file="$1"
    local update_cmake="$2"
    local new_version="$3"
    local current_version="$4"
    local do_commit="$5"
    local do_tag="$6"
    local do_push="$7"
    local push_tags="$8"
    local commit_msg="$9"
    local no_verify="${10}"
    local commit_sign="${11}"
    local tag_prefix="${12}"
    local annotated_tag="${13}"
    local signed_tag="${14}"
    local allow_dirty="${15}"
    local project_root="${16}"
    local original_project_root="${17}"
    local remote="${18:-origin}"
    local allow_nonmonotonic="${19:-false}"
    
    # Check prerequisites
    check_git_prerequisites "$do_commit" "$do_tag" "$do_push" "$push_tags"
    check_signing_prerequisites "$signed_tag" "$commit_sign"
    
    # Check dirty tree if needed
    if { is_true "$do_commit" || is_true "$do_tag"; } && is_false "$allow_dirty"; then
        check_dirty_tree
    fi
    
    # Refresh index before operations
    if is_true "$do_commit" || is_true "$do_tag"; then
        git update-index -q --refresh || true
    fi
    
    # Perform operations
    if is_true "$do_commit"; then
        create_commit "$version_file" "$update_cmake" "$new_version" "$current_version" \
                     "$commit_msg" "$no_verify" "$commit_sign" "$tag_prefix" "$project_root" "$original_project_root"
    fi
    
    if is_true "$do_tag"; then
        if is_false "$do_commit"; then
            # Warn if tagging without committing version bump
            local files_to_check=("$version_file")
            is_true "$update_cmake" && files_to_check+=("$project_root/CMakeLists.txt")
            if ! git diff --quiet -- "${files_to_check[@]}"; then
                warn "Tagging without --commit; ensure the bump commit is pushed before the tag"
            fi
            # Warn if any uncommitted changes exist
            if ! git diff --quiet; then
                warn "Uncommitted changes exist; tag may not reflect working tree"
            fi
        fi
        check_version_order "$new_version" "$tag_prefix" "$allow_nonmonotonic"
        create_tag "$new_version" "$tag_prefix" "$annotated_tag" "$signed_tag"
    fi
    
    # Push operations
    if is_true "$do_push"; then
        push_changes "$remote" "$do_tag" "$new_version" "$tag_prefix"
    fi
    
    if is_true "$push_tags"; then
        push_all_tags "$remote"
    fi
    
    # Generate summary
    generate_summary "$do_commit" "$do_tag" "$do_push" "$push_tags" "$new_version" "$tag_prefix" "$remote"
}

# --- Standalone usage --------------------------------------------------------
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # Initialize colors
    init_colors "${NO_COLOR:-false}"
    
    case "${1:-}" in
        "check-dirty")
            check_dirty_tree
            ;;
        "create-commit")
            if [[ $# -lt 9 ]]; then
                die "Usage: $(basename "$0") create-commit <version_file> <update_cmake:true|false> <new_version> <current_version> <commit_msg|''> <no_verify:true|false> <commit_sign:true|false> <tag_prefix> [project_root] [original_project_root]"
            fi
            create_commit "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9" "${10:-}" "${11:-}"
            ;;
        "create-tag")
            if [[ $# -lt 5 ]]; then
                die "Usage: $(basename "$0") create-tag <new_version> <tag_prefix> <annotated_tag:true|false> <signed_tag:true|false>"
            fi
            create_tag "$2" "$3" "$4" "$5"
            ;;
        "push-changes")
            if [[ $# -lt 5 ]]; then
                die "Usage: $(basename "$0") push-changes <remote> <do_tag:true|false> <new_version> <tag_prefix>"
            fi
            push_changes "$2" "$3" "$4" "$5"
            ;;
        "push-tags")
            if [[ $# -lt 2 ]]; then
                die "Usage: $(basename "$0") push-tags <remote>"
            fi
            push_all_tags "$2"
            ;;
        "perform-git-operations")
            if [[ $# -lt 19 ]]; then
                die "Usage: $(basename "$0") perform-git-operations <version_file> <update_cmake> <new_version> <current_version> <do_commit> <do_tag> <do_push> <push_tags> <commit_msg> <no_verify> <commit_sign> <tag_prefix> <annotated_tag> <signed_tag> <allow_dirty> <project_root> <original_project_root> [remote] [allow_nonmonotonic]"
            fi
            perform_git_operations "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9" "${10}" "${11}" "${12}" "${13}" "${14}" "${15}" "${16}" "${17}" "${18:-origin}" "${19:-false}"
            ;;
        *)
            cat <<'EOF'
Usage: git-operations.sh <command> [args...]

Commands:
  check-dirty                                    Check if working tree is dirty (excluding VERSION and optionally CMakeLists.txt)
  create-commit <args...>                        Create a commit with version bump
  create-tag <args...>                           Create a tag for the new version (pre-releases are rejected)
  push-changes <remote> <do_tag> <new_ver> <pfx> Push current branch; if do_tag=true, performs an --atomic push of branch+tag
  push-tags <remote>                             Push all tags
  perform-git-operations <args...>               Perform all git operations (commit, tag, push) in sequence

Examples:
  git-operations.sh check-dirty
  git-operations.sh create-commit VERSION true 1.0.1 1.0.0 "" false false v /path/to/project
  git-operations.sh create-tag 1.0.1 v true false
  git-operations.sh push-changes origin true 1.0.1 v
  git-operations.sh push-tags origin
  git-operations.sh perform-git-operations VERSION false 1.0.1 1.0.0 true true true false "" false false v false false false /path/to/project /path/to/project origin false
EOF
            exit 1
            ;;
    esac
fi 