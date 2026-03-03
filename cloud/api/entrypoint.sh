#!/bin/bash
set -e

echo "🚀 Starting LoRaWAN Backend..."

# Start Gunicorn
exec gunicorn config.wsgi:application --bind 0.0.0.0:8000 --workers 4 --timeout 120
