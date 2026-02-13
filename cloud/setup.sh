#!/bin/bash

# Setup script for LoRaWAN Cloud Backend

set -e

echo "🚀 Starting Django setup..."

# Run migrations
echo "📦 Running migrations..."
python manage.py migrate

# Create superuser if it doesn't exist
echo "👤 Creating superuser..."
python manage.py shell << END
from django.contrib.auth import get_user_model
User = get_user_model()
if not User.objects.filter(username='admin').exists():
    User.objects.create_superuser('admin', 'admin@lorawan.local', 'admin')
    print("✅ Superuser 'admin' created")
else:
    print("✅ Superuser already exists")
END

# Create test device
echo "📱 Creating test device..."
python manage.py shell << END
from django.contrib.auth import get_user_model
from devices.models import Device
User = get_user_model()
admin = User.objects.get(username='admin')

device, created = Device.objects.get_or_create(
    device_id='test_device_001',
    defaults={
        'name': 'Test Sensor',
        'description': 'Test device for development',
        'dev_eui': '70B3D57ED0041234',
        'device_type': 'sensor',
        'owner': admin,
        'latitude': 48.8566,
        'longitude': 2.3522,
    }
)

if created:
    print(f"✅ Test device created: {device.name}")
else:
    print(f"✅ Test device already exists: {device.name}")
END

echo "✅ Setup complete!"
echo ""
echo "📝 Credentials:"
echo "   Username: admin"
echo "   Password: admin"
echo ""
echo "🌐 Access URLs:"
echo "   Admin Panel: http://localhost:8000/admin"
echo "   API: http://localhost:8000/api"
