#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/../../cloud" && pwd)"
CLOUD_ENV_FILE="${PROJECT_DIR}/.env"

# shellcheck source=../lib/lib.sh
source "${SCRIPT_DIR}/../lib/lib.sh"

# ---------------------------------------------------------------------------
# install_docker
#   Installs Docker CE (with compose plugin) for the detected distro.
#   Also enables and starts the Docker daemon via systemctl when available.
# ---------------------------------------------------------------------------
install_docker() {
    echo "Docker not found. Installing Docker..."
    case "$PKG_MANAGER" in
        apt-get)
            # Needs GPG repo setup and CE-specific package names
            sudo apt-get update
            sudo apt-get install -y ca-certificates curl gnupg lsb-release
            . /etc/os-release
            local distro_id="${ID:-ubuntu}"
            local distro_codename="${VERSION_CODENAME:-$(lsb_release -cs)}"
            sudo mkdir -p /etc/apt/keyrings
            curl -fsSL "https://download.docker.com/linux/${distro_id}/gpg" \
                | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
            echo \
                "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg]" \
                "https://download.docker.com/linux/${distro_id} ${distro_codename} stable" \
                | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
            sudo apt-get update
            sudo apt-get install -y docker-ce docker-ce-cli containerd.io \
                docker-buildx-plugin docker-compose-plugin
            ;;
        yum)
            # Needs repo setup and CE-specific package names
            sudo yum install -y yum-utils
            sudo yum-config-manager \
                --add-repo https://download.docker.com/linux/centos/docker-ce.repo || true
            sudo yum install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin \
                || sudo yum install -y docker docker-compose
            ;;
        dnf)
            # Needs repo setup and CE-specific package names
            sudo dnf -y install dnf-plugins-core
            sudo dnf config-manager \
                --add-repo https://download.docker.com/linux/fedora/docker-ce.repo || true
            sudo dnf install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin \
                || sudo dnf install -y docker docker-compose
            ;;
        apk)
            # Non-standard compose package name
            install_package docker
            install_package docker-cli-compose
            ;;
        *)
            # pacman, zypper, snap: standard names
            install_package docker
            install_package docker-compose
            ;;
    esac
    if command -v systemctl &>/dev/null; then
        sudo systemctl enable docker || true
        sudo systemctl start docker  || true
    fi
    echo "Docker installed successfully."
}

# ---------------------------------------------------------------------------
# resolve_compose_cmd
#   Sets and exports COMPOSE_CMD to "docker compose" or "docker-compose".
#   Installs Docker Compose if neither variant is available.
# ---------------------------------------------------------------------------
resolve_compose_cmd() {
    if docker compose version &>/dev/null; then
        COMPOSE_CMD="docker compose"
    elif command -v docker-compose &>/dev/null; then
        COMPOSE_CMD="docker-compose"
    else
        echo "Docker Compose not available. Installing..."
        case "$PKG_MANAGER" in
            apt-get)
                # Tries plugin first, falls back to standalone
                sudo apt-get update
                sudo apt-get install -y docker-compose-plugin \
                    || sudo apt-get install -y docker-compose
                ;;
            yum)  sudo yum install -y docker-compose-plugin || sudo yum install -y docker-compose ;;
            dnf)  sudo dnf install -y docker-compose-plugin || sudo dnf install -y docker-compose ;;
            apk)  install_package docker-cli-compose ;; # non-standard name
            snap) echo "Docker Compose is expected to be bundled with the Docker snap." ;;
            *)    install_package docker-compose ;;
        esac
        if docker compose version &>/dev/null; then
            COMPOSE_CMD="docker compose"
        elif command -v docker-compose &>/dev/null; then
            COMPOSE_CMD="docker-compose"
        else
            echo "Error: failed to install Docker Compose." >&2; exit 1
        fi
    fi
    export COMPOSE_CMD
}

# ---------------------------------------------------------------------------
# install_certbot
#   Installs certbot for the detected distro.
#   On apt-get, falls back to snap if the apt package is unavailable.
#   On snap, installs via snap with the --classic flag and creates a symlink.
# ---------------------------------------------------------------------------
install_certbot() {
    if command -v certbot &>/dev/null; then return; fi
    echo "Certbot not found. Installing certbot..."
    case "$PKG_MANAGER" in
        snap)
            sudo snap install core && sudo snap refresh core
            sudo snap install --classic certbot
            sudo ln -sf /snap/bin/certbot /usr/bin/certbot
            ;;
        *)
            install_package certbot
            # apt-get: certbot package may be absent; fall back to snap
            if ! command -v certbot &>/dev/null && command -v snap &>/dev/null; then
                echo "Falling back to snap for certbot..."
                sudo snap install core && sudo snap refresh core
                sudo snap install --classic certbot
                sudo ln -sf /snap/bin/certbot /usr/bin/certbot
            fi
            ;;
    esac
    if ! command -v certbot &>/dev/null; then
        echo "Error: failed to install certbot. Please install it manually." >&2
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Bootstrap: always executed when this file is sourced.
# ---------------------------------------------------------------------------
load_env "$CLOUD_ENV_FILE"
require_pkg_manager
