from django.contrib import admin
from .models import Device, DeviceMetadata

@admin.register(Device)
class DeviceAdmin(admin.ModelAdmin):
    list_display = ['name', 'device_id', 'dev_eui', 'device_type', 'owner', 'is_active', 'created_at']
    list_filter = ['device_type', 'is_active', 'created_at']
    search_fields = ['name', 'device_id', 'dev_eui']
    readonly_fields = ['created_at', 'updated_at']

@admin.register(DeviceMetadata)
class DeviceMetadataAdmin(admin.ModelAdmin):
    list_display = ['device', 'created_at', 'updated_at']
    readonly_fields = ['created_at', 'updated_at']
