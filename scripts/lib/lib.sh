#!/bin/bash
# lib.sh — Shared utilities for all project scripts.
# Source this file; do not execute it directly.
#
# The caller must set SCRIPT_DIR before sourcing:
#   SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ---------------------------------------------------------------------------
# detect_pkg_manager
#   Prints the first available package manager name, or nothing.
#   Supported: apt-get · yum · dnf · pacman · zypper · apk · snap
# ---------------------------------------------------------------------------
detect_pkg_manager() {
    if   command -v apt-get &>/dev/null; then echo "apt-get"
    elif command -v yum     &>/dev/null; then echo "yum"
    elif command -v dnf     &>/dev/null; then echo "dnf"
    elif command -v pacman  &>/dev/null; then echo "pacman"
    elif command -v zypper  &>/dev/null; then echo "zypper"
    elif command -v apk     &>/dev/null; then echo "apk"
    elif command -v snap    &>/dev/null; then echo "snap"
    fi
}

# ---------------------------------------------------------------------------
# require_pkg_manager
#   Detects and exports PKG_MANAGER (idempotent — skips if already set).
# ---------------------------------------------------------------------------
require_pkg_manager() {
    if [ -n "${PKG_MANAGER:-}" ]; then return; fi
    PKG_MANAGER="$(detect_pkg_manager)"
    if [ -z "$PKG_MANAGER" ]; then
        echo "Error: unsupported distribution — no known package manager found." >&2
        exit 1
    fi
    export PKG_MANAGER
}

# ---------------------------------------------------------------------------
# install_package <pkg> [extra_snap_args...]
#   Installs <pkg> using the detected package manager.
#   Extra args (e.g. --classic) are forwarded to snap install only.
# ---------------------------------------------------------------------------
install_package() {
    local pkg="$1"; shift
    local snap_extra=("$@")
    require_pkg_manager
    echo "Installing ${pkg}..."
    case "$PKG_MANAGER" in
        apt-get) sudo apt-get update -qq && sudo apt-get install -y "$pkg" ;;
        yum)     sudo yum install -y "$pkg" ;;
        dnf)     sudo dnf install -y "$pkg" ;;
        pacman)  sudo pacman -S --noconfirm "$pkg" ;;
        zypper)  sudo zypper install -y "$pkg" ;;
        apk)     sudo apk add "$pkg" ;;
        snap)    sudo snap install "${snap_extra[@]}" "$pkg" ;;
    esac
}

# ---------------------------------------------------------------------------
# load_env [env_file_path]
#   Sources the .env file at the given path (defaults to SCRIPT_DIR/../.env),
#   then validates that DOMAIN is set.
#   Exports: DOMAIN (and all other variables from .env).
# ---------------------------------------------------------------------------
load_env() {
    local env_file="${1:-${SCRIPT_DIR}/../.env}"
    if [ ! -f "$env_file" ]; then
        echo "Error: ${env_file} not found. Please create it from .env.template." >&2
        exit 1
    fi
    # shellcheck source=/dev/null
    source "$env_file"
    if [ -z "$DOMAIN" ]; then
        echo "Error: DOMAIN is not defined in ${env_file}." >&2
        exit 1
    fi
    echo "Found .env file. DOMAIN=$DOMAIN"
    export DOMAIN
}
