#!/bin/bash

# This script checks for the .env file, 
# ensures Docker and Docker Compose are installed, 
# and then starts the services defined in the docker-compose.yml 
# file.

set -e

# Define the path to the .env file
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_FILE="$SCRIPT_DIR/../.env"

# Check for .env file
if [ ! -f "$ENV_FILE" ]; then
    echo "Error: $ENV_FILE file not found. Please create it with the necessary environment variables."
    exit 1
fi

source "$ENV_FILE"

if [ -z "$DOMAIN" ]; then
    echo "Error: the DOMAIN variable is not defined in $ENV_FILE."
    exit 1
fi

echo "Found .env file. DOMAIN=$DOMAIN"

# Install Docker if not present
if ! command -v docker &> /dev/null; then
    echo "Docker not found. Installing Docker..."
    sudo apt update
    sudo apt install -y ca-certificates curl gnupg lsb-release

    sudo mkdir -p /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
    echo \
        "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
        $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

    sudo apt update
    sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

    echo "Docker installed successfully."
fi

# Verify Docker Compose plugin
if ! docker compose version &> /dev/null; then
    echo "Docker Compose plugin not available. Installing Compose plugin..."
    sudo apt install -y docker-compose-plugin
fi

# Parse daemon flag
DAEMON=""
for arg in "$@"; do
    if [ "$arg" = "--daemon" ] || [ "$arg" = "-d" ]; then
        DAEMON="-d"
        break
    fi
done

# Launch services
echo "Starting services..."
docker compose up --build $DAEMON