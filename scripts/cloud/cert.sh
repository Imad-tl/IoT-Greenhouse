#!/bin/bash
# cert.sh — Verify .env, install certbot if absent,
#            then generate an SSL certificate for DOMAIN.

set -e

# shellcheck source=_commons.sh
source "$(cd "$(dirname "$0")" && pwd)/_commons.sh"

install_certbot

echo "Generating SSL certificate for ${DOMAIN}..."
certbot certonly --standalone -d "$DOMAIN" \
    --non-interactive --agree-tos --register-unsafely-without-email

echo "Certificate generation completed!"
