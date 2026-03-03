#!/bin/bash
# start.sh — Verify .env, install Docker / Docker Compose if absent,
#             then launch the docker-compose stack.
# Usage: ./scripts/cloud/start.sh [--daemon|-d]

set -e

# shellcheck source=_commons.sh
source "$(cd "$(dirname "$0")" && pwd)/_commons.sh"

command -v docker &>/dev/null || install_docker
resolve_compose_cmd

# Parse flags
DAEMON=""
for arg in "$@"; do
    if [ "$arg" = "--daemon" ] || [ "$arg" = "-d" ]; then
        DAEMON="-d"
        break
    fi
done

echo "Starting services..."
cd "$PROJECT_DIR"
$COMPOSE_CMD up --build $DAEMON
