#!/bin/bash
# install.sh — Bootstrap: installs make on the host machine.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# shellcheck source=lib/lib.sh
source "${SCRIPT_DIR}/lib/lib.sh"

install_package make

echo "make installed successfully"
