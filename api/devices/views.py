from rest_framework import viewsets, permissions, status
from rest_framework.decorators import action
from rest_framework.response import Response
from .models import Device, DeviceMetadata
from .serializers import DeviceSerializer

class DeviceViewSet(viewsets.ModelViewSet):
    serializer_class = DeviceSerializer
    permission_classes = [permissions.IsAuthenticated]
    
    def get_queryset(self):
        return Device.objects.filter(owner=self.request.user)
    
    def perform_create(self, serializer):
        serializer.save(owner=self.request.user)
    
    @action(detail=True, methods=['post'])
    def set_metadata(self, request, pk=None):
        """Set device metadata (fields, units, etc.)"""
        device = self.get_object()
        fields = request.data.get('fields', {})
        
        metadata, created = DeviceMetadata.objects.get_or_create(device=device)
        metadata.fields = fields
        metadata.save()
        
        return Response({'status': 'metadata updated'}, status=status.HTTP_200_OK)
    
    @action(detail=True, methods=['get'])
    def last_data(self, request, pk=None):
        """Get last data point from InfluxDB"""
        from ingestion.influxdb_client import InfluxDBClient
        device = self.get_object()
        
        influx = InfluxDBClient()
        data = influx.get_last_data(device.device_id)
        
        return Response(data)
