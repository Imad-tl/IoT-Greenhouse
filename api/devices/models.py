from django.db import models
from django.contrib.auth.models import User

class Device(models.Model):
    DEVICE_TYPES = [
        ('sensor', 'Sensor'),
        ('gateway', 'Gateway'),
        ('actuator', 'Actuator'),
    ]
    
    device_id = models.CharField(max_length=100, unique=True)
    name = models.CharField(max_length=255)
    description = models.TextField(blank=True)
    device_type = models.CharField(max_length=20, choices=DEVICE_TYPES, default='sensor')
    
    # LoRaWAN specific
    dev_eui = models.CharField(max_length=16, unique=True)
    app_eui = models.CharField(max_length=16, blank=True)
    
    # Location
    latitude = models.FloatField(null=True, blank=True)
    longitude = models.FloatField(null=True, blank=True)
    
    owner = models.ForeignKey(User, on_delete=models.CASCADE)
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)
    is_active = models.BooleanField(default=True)
    
    class Meta:
        ordering = ['-created_at']
    
    def __str__(self):
        return f"{self.name} ({self.device_id})"


class DeviceMetadata(models.Model):
    """Store sensor metadata like field descriptions, units, etc."""
    device = models.OneToOneField(Device, on_delete=models.CASCADE, related_name='metadata')
    fields = models.JSONField(default=dict, help_text='e.g., {"temperature": {"unit": "°C", "type": "float"}}')
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)
    
    def __str__(self):
        return f"Metadata for {self.device.name}"
