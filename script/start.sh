#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_FILE="$SCRIPT_DIR/../.env"

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
echo "Starting services..."

docker compose up --build
