from rest_framework import viewsets, permissions, status
from rest_framework.decorators import action
from rest_framework.response import Response
from devices.models import Device
from ingestion.influxdb_client import InfluxDBClient
from datetime import datetime, timedelta

class SensorDataViewSet(viewsets.ViewSet):
    permission_classes = [permissions.IsAuthenticated]
    
    def list(self, request):
        """List all available devices for user"""
        devices = Device.objects.filter(owner=request.user)
        data = [{'id': d.id, 'device_id': d.device_id, 'name': d.name} for d in devices]
        return Response(data)
    
    @action(detail=False, methods=['get'])
    def device_data(self, request):
        """Get data for a specific device
        
        Query params:
        - device_id: Device ID (required)
        - range: Time range in hours (default: 24)
        """
        device_id = request.query_params.get('device_id')
        time_range = int(request.query_params.get('range', 24))
        
        if not device_id:
            return Response(
                {'error': 'device_id parameter required'},
                status=status.HTTP_400_BAD_REQUEST
            )
        
        try:
            device = Device.objects.get(device_id=device_id, owner=request.user)
            
            influx = InfluxDBClient()
            query = f'''
            from(bucket:"{influx.client._conf.bucket}")
              |> range(start: -{time_range}h)
              |> filter(fn: (r) => r.device_id == "{device_id}")
            '''
            result = influx.query_api.query(org=influx.client._conf.org, query=query)
            
            data = []
            for table in result:
                for record in table.records:
                    data.append({
                        'timestamp': record.get_time().isoformat(),
                        'field': record.field,
                        'value': record.value,
                        'tags': record.tags
                    })
            
            influx.close()
            return Response({'data': data})
        
        except Device.DoesNotExist:
            return Response(
                {'error': 'Device not found'},
                status=status.HTTP_404_NOT_FOUND
            )
        except Exception as e:
            return Response(
                {'error': str(e)},
                status=status.HTTP_500_INTERNAL_SERVER_ERROR
            )
    
    @action(detail=False, methods=['get'])
    def aggregation(self, request):
        """Get aggregated data
        
        Query params:
        - device_id: Device ID (required)
        - aggregation: mean, sum, min, max (default: mean)
        - interval: Time interval like 1h, 10m (default: 1h)
        - range: Time range in hours (default: 24)
        """
        device_id = request.query_params.get('device_id')
        aggregation = request.query_params.get('aggregation', 'mean')
        interval = request.query_params.get('interval', '1h')
        time_range = int(request.query_params.get('range', 24))
        
        if not device_id:
            return Response(
                {'error': 'device_id parameter required'},
                status=status.HTTP_400_BAD_REQUEST
            )
        
        try:
            device = Device.objects.get(device_id=device_id, owner=request.user)
            
            influx = InfluxDBClient()
            query = f'''
            from(bucket:"{influx.client._conf.bucket}")
              |> range(start: -{time_range}h)
              |> filter(fn: (r) => r.device_id == "{device_id}")
              |> aggregateWindow(every: {interval}, fn: {aggregation}, createEmpty: false)
            '''
            result = influx.query_api.query(org=influx.client._conf.org, query=query)
            
            data = []
            for table in result:
                for record in table.records:
                    data.append({
                        'timestamp': record.get_time().isoformat(),
                        'field': record.field,
                        'value': record.value
                    })
            
            influx.close()
            return Response({'data': data})
        
        except Device.DoesNotExist:
            return Response(
                {'error': 'Device not found'},
                status=status.HTTP_404_NOT_FOUND
            )
        except Exception as e:
            return Response(
                {'error': str(e)},
                status=status.HTTP_500_INTERNAL_SERVER_ERROR
            )
