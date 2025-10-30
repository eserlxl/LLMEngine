#!/bin/bash
# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of nextVersion and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.
#
# CI GPG key generator for next-version
# Generates GPG keys for CI/CD signing

set -Eeuo pipefail
IFS=$'\n\t'
export LC_ALL=C

# Source common utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/version-utils.sh"

# Initialize colors
init_colors

# ---------- appearance & helpers ----------
is_tty=0; [[ -t 1 ]] && is_tty=1
# shellcheck disable=SC2034  # some color vars might be unused depending on TTY
c_red=$'\033[0;31m'; c_grn=$'\033[0;32m'; c_ylw=$'\033[1;33m'; c_cyn=$'\033[0;36m'; c_rst=$'\033[0m'
colorize() { if (( is_tty )); then printf '%s' "${1}"; fi; }
say()  { colorize "${c_cyn}";  printf '[*] %s\n' "$*"; colorize "${c_rst}"; }
ok()   { colorize "${c_grn}";  printf '[✓] %s\n' "$*"; colorize "${c_rst}"; }
warn() { colorize "${c_ylw}";  printf '[!] %s\n' "$*"; colorize "${c_rst}"; }

# ---------- defaults ----------
NAME=""
EMAIL=""
COMMENT=""
EXPIRE="2y"
ALGO="ed25519"     # ed25519 | rsa4096
OUT_DIR="./ci-gpg-out"
PRINT_SECRETS=0
WRITE_B64=1        # also produce single-line base64 exports
PASSPHRASE=""      # generated if empty
UMASK_SET=077
WITH_SUBKEY=0      # create signing subkey with cert-only primary (recommended)
MAKE_REVOKE=0      # write revocation-cert.asc

# ---------- utilities ----------
# die() and require_cmd() functions are now sourced from version-utils.sh

abs_path() {
  # Portable absolute path (macOS lacks readlink -f, sometimes no realpath)
  if command -v realpath >/dev/null 2>&1; then realpath "$1" && return; fi
  if readlink -f / >/dev/null 2>&1; then readlink -f "$1" && return; fi
  # Fallback: prepend PWD if relative
  case "$1" in
    /*) printf '%s\n' "$1";;
    *)  printf '%s/%s\n' "$(pwd -P)" "$1";;
  esac
}

gen_passphrase() {
  if [[ -n "$PASSPHRASE" ]]; then return; fi
  if command -v openssl >/dev/null 2>&1; then
    PASSPHRASE="$(openssl rand -base64 32)"
  else
    PASSPHRASE="$(head -c 32 /dev/urandom | base64)"
  fi
}

supports_ed25519() {
  gpg --version 2>/dev/null | grep -qi 'ed25519' && return 0 || return 1
}

to_oneline_b64() {
  # GNU base64 has -w/--wrap; BSD base64 doesn't.
  if base64 --help 2>&1 | grep -qE -- '(-w|--wrap)'; then
    base64 -w 0
  else
    base64 | tr -d '\n'
  fi
}

# ---------- interactive prompts ----------
prompt_for_identity() {
  local input_name="" input_email="" input_comment=""
  
  echo "=== GPG Key Identity Setup ==="
  echo "Please provide the identity information for your GPG key."
  echo "This will be used to sign commits and should match your GitHub identity."
  echo ""
  
  # Prompt for name
  while [[ -z "$input_name" ]]; do
    read -r -p "Enter your full name (e.g., 'John Doe'): " input_name
    if [[ -z "$input_name" ]]; then
      echo "Name cannot be empty. Please try again."
    fi
  done
  
  # Prompt for email
  while [[ -z "$input_email" ]]; do
    read -r -p "Enter your email address (e.g., 'john.doe@example.com'): " input_email
    if [[ -z "$input_email" ]]; then
      echo "Email cannot be empty. Please try again."
    elif ! echo "$input_email" | grep -qE '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$'; then
      echo "Please enter a valid email address."
      input_email=""
    fi
  done
  
  # Prompt for comment
  read -r -p "Enter a comment (optional, e.g., 'CI signing key'): " input_comment
  
  # Set the values
  NAME="$input_name"
  EMAIL="$input_email"
  COMMENT="${input_comment:-CI signing key for nextVersion}"
  
  echo ""
  echo "=== Identity Summary ==="
  echo "Name: $NAME"
  echo "Email: $EMAIL"
  echo "Comment: $COMMENT"
  echo ""
  
  # Confirm with user
  read -r -p "Is this correct? (y/N): " confirm
  if [[ "$confirm" =~ ^[Yy]$ ]]; then
    echo "Identity confirmed. Proceeding with key generation..."
  else
    echo "Identity not confirmed. Exiting."
    exit 1
  fi
}

# ---------- usage ----------
usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --name STR           Real name (will prompt if not provided)
  --email STR          Email (will prompt if not provided)
  --comment STR        Comment (will prompt if not provided)
  --expire STR         Expire date (default: "$EXPIRE", e.g. 1y, 0 for never)
  --algo ALG           Key algorithm: ed25519 | rsa4096 (default: $ALGO)
  --with-subkey        Create a signing subkey; primary is certification-only
  --revoke-cert        Also generate revocation-cert.asc
  --out-dir PATH       Output directory (default: $OUT_DIR)
  --no-b64             Do not create base64 single-line copies
  --print-secrets      Also print secret key & passphrase to stdout (NOT default)
  --passphrase STR     Use provided passphrase (otherwise a random one is created)
  -h, --help           Show this help

Note: If --name, --email, or --comment are not provided, the script will
      interactively prompt for these values to ensure proper GitHub verification.

Outputs (in --out-dir):
  public.asc           ASCII-armored public key
  secret.asc           ASCII-armored private key (protected with passphrase)
  passphrase.txt       Passphrase (mode 600)
  public.asc.b64       Base64 (single-line) of public.asc      [optional]
  secret.asc.b64       Base64 (single-line) of secret.asc      [optional]
  revocation-cert.asc  Revocation certificate                   [optional]

Recommended CI usage:
  - Put secret.asc (or secret.asc.b64) into GitHub Secret GPG_PRIVATE_KEY
  - Put public.asc (or public.asc.b64) into GitHub Variable/Secret GPG_PUBLIC_KEY
  - Put passphrase.txt into GitHub Secret GPG_PASSPHRASE
EOF
}

# ---------- parse args ----------
while (( $# )); do
  case "$1" in
    --name)        NAME=${2:?}; shift 2;;
    --email)       EMAIL=${2:?}; shift 2;;
    --comment)     COMMENT=${2:?}; shift 2;;
    --expire)      EXPIRE=${2:?}; shift 2;;
    --algo)        ALGO=${2:?}; shift 2;;
    --with-subkey) WITH_SUBKEY=1; shift;;
    --revoke-cert) MAKE_REVOKE=1; shift;;
    --out-dir)     OUT_DIR=${2:?}; shift 2;;
    --no-b64)      WRITE_B64=0; shift;;
    --print-secrets) PRINT_SECRETS=1; shift;;
    --passphrase)  PASSPHRASE=${2:?}; shift 2;;
    -h|--help)     usage; exit 0;;
    --) shift; break;;
    *) die "Unknown option: $1 (use --help)";;
  esac
done

# ---------- preflight ----------
umask "$UMASK_SET"
require_cmd gpg
gen_passphrase

# Prompt for identity if not provided via command line
if [[ -z "$NAME" ]] || [[ -z "$EMAIL" ]]; then
  prompt_for_identity
fi

# Validate required fields
[[ -n "$NAME" ]] || die "Name is required"
[[ -n "$EMAIL" ]] || die "Email is required"
COMMENT="${COMMENT:-CI signing key for nextVersion}"

# Prefer ed25519; fall back to RSA if unsupported
if [[ "$ALGO" == "ed25519" ]] && ! supports_ed25519; then
  warn "This GnuPG build seems to lack Ed25519 support; falling back to RSA-4096."
  ALGO="rsa4096"
fi

mkdir -p "$OUT_DIR"
chmod 700 "$OUT_DIR" || true

# temp workdir for isolated GNUPGHOME
TMP_ROOT="$(mktemp -d)"
cleanup() {
  local ec=$?
  # Kill any gpg-agent in this GNUPGHOME
  if [[ -n "${GNUPGHOME:-}" && -d "$GNUPGHOME" ]]; then
    gpgconf --kill gpg-agent >/dev/null 2>&1 || true
  fi
  rm -rf "$TMP_ROOT"
  exit "$ec"
}
trap cleanup EXIT INT TERM

export GNUPGHOME="$TMP_ROOT/gnupg"
mkdir -p "$GNUPGHOME"; chmod 700 "$GNUPGHOME"

# Enable loopback pinentry for non-interactive export
printf 'allow-loopback-pinentry\nmax-cache-ttl 1\n' > "$GNUPGHOME/gpg-agent.conf"
printf 'batch\npinentry-mode loopback\nno-tty\n' > "$GNUPGHOME/gpg.conf"

# ---------- build batch file ----------
BATCH="$(mktemp "$TMP_ROOT/gpg-batch.XXXX")"
case "$ALGO" in
  ed25519)
    if (( WITH_SUBKEY )); then
      cat >"$BATCH" <<EOF
%echo Generating CI GPG key (primary cert-only + signing subkey)
Key-Type: eddsa
Key-Curve: ed25519
Key-Usage: cert
Name-Real: ${NAME}
Name-Email: ${EMAIL}
Name-Comment: ${COMMENT}
Expire-Date: ${EXPIRE}
Passphrase: ${PASSPHRASE}
Subkey-Type: eddsa
Subkey-Curve: ed25519
Subkey-Usage: sign
%commit
%echo Done
EOF
    else
      cat >"$BATCH" <<EOF
%echo Generating CI GPG key (primary sign-only)
Key-Type: eddsa
Key-Curve: ed25519
Key-Usage: sign
Name-Real: ${NAME}
Name-Email: ${EMAIL}
Name-Comment: ${COMMENT}
Expire-Date: ${EXPIRE}
Passphrase: ${PASSPHRASE}
%commit
%echo Done
EOF
    fi
    ;;
  rsa4096)
    if (( WITH_SUBKEY )); then
      cat >"$BATCH" <<EOF
%echo Generating CI GPG key (primary cert-only + signing subkey)
Key-Type: RSA
Key-Length: 4096
Key-Usage: cert
Name-Real: ${NAME}
Name-Email: ${EMAIL}
Name-Comment: ${COMMENT}
Expire-Date: ${EXPIRE}
Passphrase: ${PASSPHRASE}
Subkey-Type: RSA
Subkey-Length: 4096
Subkey-Usage: sign
%commit
%echo Done
EOF
    else
      cat >"$BATCH" <<EOF
%echo Generating CI GPG key (primary sign-only)
Key-Type: RSA
Key-Length: 4096
Key-Usage: sign
Name-Real: ${NAME}
Name-Email: ${EMAIL}
Name-Comment: ${COMMENT}
Expire-Date: ${EXPIRE}
Passphrase: ${PASSPHRASE}
%commit
%echo Done
EOF
    fi
    ;;
  *)
    die "Unsupported --algo '$ALGO'. Use ed25519 or rsa4096."
    ;;
esac

say "Generating GPG key ($ALGO, expires ${EXPIRE})…"
gpg --batch --generate-key "$BATCH"

# ---------- identify key (fingerprint & keyid) ----------
FPR="$(gpg --batch --with-colons --list-secret-keys "$EMAIL" | awk -F: '/^fpr:/{print $10; exit}')"
[[ -n "$FPR" ]] || die "Failed to locate generated key fingerprint"
KEYID="${FPR:(-16)}"

ok "Key generated"
say "Key ID (long): $KEYID"
say "Fingerprint    : $FPR"

# ---------- exports ----------
PUB_ASC="$OUT_DIR/public.asc"
SEC_ASC="$OUT_DIR/secret.asc"
PASS_TXT="$OUT_DIR/passphrase.txt"

say "Exporting armored public/secret keys…"
gpg --armor --export "$FPR" > "$PUB_ASC"
gpg --batch --pinentry-mode loopback --passphrase "$PASSPHRASE" --armor --export-secret-keys "$FPR" > "$SEC_ASC"

[[ -s "$PUB_ASC" && -s "$SEC_ASC" ]] || die "Export failed"

printf '%s\n' "$PASSPHRASE" > "$PASS_TXT"
chmod 600 "$PASS_TXT" || true

if (( WRITE_B64 )); then
  say "Creating single-line base64 copies…"
  <"$PUB_ASC" to_oneline_b64 > "$OUT_DIR/public.asc.b64"
  <"$SEC_ASC" to_oneline_b64 > "$OUT_DIR/secret.asc.b64"
fi

# ---------- optional: revocation cert ----------
if (( MAKE_REVOKE )); then
  say "Generating revocation certificate…"
  REVOKE="$OUT_DIR/revocation-cert.asc"
  # Use --command-fd to answer prompts non-interactively
  # Sequence: confirm=y, reason=0 (no reason), description='', confirm=y
  { printf 'y\n0\n\n' ; printf 'y\n'; } | \
    gpg --batch --yes --pinentry-mode loopback --passphrase "$PASSPHRASE" \
        --command-fd 0 --status-fd 2 \
        --output "$REVOKE" --gen-revoke "$FPR" >/dev/null 2>&1 || warn "Revocation cert generation failed; you can create it later with gpg --gen-revoke"
  [[ -s "$REVOKE" ]] && ok "Revocation certificate written: $(abs_path "$REVOKE")"
fi

# ---------- summary ----------
ok "GPG Key Setup Complete"
cat <<EOF
Outputs written to: $(abs_path "$OUT_DIR")

  - Public key  : $(abs_path "$PUB_ASC")
  - Private key : $(abs_path "$SEC_ASC")
  - Passphrase  : $(abs_path "$PASS_TXT")
$( (( WRITE_B64 )) && printf '  - Public (b64): %s\n' "$(abs_path "$OUT_DIR/public.asc.b64")" )
$( (( WRITE_B64 )) && printf '  - Secret (b64): %s\n' "$(abs_path "$OUT_DIR/secret.asc.b64")" )
$( (( MAKE_REVOKE )) && [[ -f "$OUT_DIR/revocation-cert.asc" ]] && printf '  - Revocation : %s\n' "$(abs_path "$OUT_DIR/revocation-cert.asc")" )

GitHub setup (recommended):
  • Secrets:
      GPG_PRIVATE_KEY  -> contents of secret.asc   (or secret.asc.b64)
      GPG_PASSPHRASE   -> contents of passphrase.txt
  • Variable/Secret:
      GPG_PUBLIC_KEY   -> contents of public.asc   (or public.asc.b64)

Local Git signing (optional):
  git config --global gpg.program gpg
  git config --global commit.gpgsign true
  git config --global user.signingkey $KEYID
EOF

if (( PRINT_SECRETS )); then
  warn "Printing secrets to stdout because --print-secrets was used."
  printf '\n-----8<----- GPG PRIVATE KEY (secret.asc) -----8<-----\n'
  cat "$SEC_ASC"
  printf '\n-----8<----- GPG PUBLIC KEY (public.asc)  -----8<-----\n'
  cat "$PUB_ASC"
  printf '\n-----8<----- PASSPHRASE --------------------8<-----\n%s\n' "$PASSPHRASE"
else
  warn "Secrets NOT printed. Use --print-secrets if you explicitly want them on stdout."
fi 