#!/bin/bash

# This script checks for the certbot installation and generates an 
# SSL certificate for the domain specified in the .env file.

set -e

ENV_FILE="../.env"

# Vérifie le fichier .env
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

# Vérifie si certbot est installé
if ! command -v certbot &> /dev/null; then

    echo "Certbot not found. Installing certbot..."
    
    # Détecte le gestionnaire de paquets
    if command -v apt &> /dev/null; then
        sudo apt update
        sudo apt install -y certbot

    if command -v snap &> /dev/null; then
        echo "Installing certbot via snap..."
        sudo snap install core
        sudo snap refresh core
        sudo snap install --classic certbot
        sudo ln -sf /snap/bin/certbot /usr/bin/certbot

    elif command -v dnf &> /dev/null; then
        sudo dnf install -y certbot

    elif command -v pacman &> /dev/null; then
        sudo pacman -S --noconfirm certbot

    else
        echo "Unsupported OS. Please install certbot manually."
        exit 1

    fi

fi

echo "Generating SSL certificate for $DOMAIN..."

certbot certonly --standalone -d "$DOMAIN" --non-interactive --agree-tos --register-unsafely-without-email

echo "Certificate generation completed!"