from rest_framework import serializers
from .models import Device, DeviceMetadata

class DeviceMetadataSerializer(serializers.ModelSerializer):
    class Meta:
        model = DeviceMetadata
        fields = ['fields', 'created_at', 'updated_at']

class DeviceSerializer(serializers.ModelSerializer):
    metadata = DeviceMetadataSerializer(read_only=True)
    
    class Meta:
        model = Device
        fields = [
            'id', 'device_id', 'name', 'description', 'device_type',
            'dev_eui', 'app_eui', 'latitude', 'longitude',
            'owner', 'created_at', 'updated_at', 'is_active', 'metadata'
        ]
        read_only_fields = ['id', 'created_at', 'updated_at', 'owner']
