from influxdb_client import InfluxDBClient as InfluxDBClientSDK
from influxdb_client.client.write_api import SYNCHRONOUS
from django.conf import settings

class InfluxDBClient:
    """Client for writing data to InfluxDB"""
    
    def __init__(self):
        self.client = InfluxDBClientSDK(
            url=settings.INFLUXDB_URL,
            token=settings.INFLUXDB_TOKEN,
            org=settings.INFLUXDB_ORG
        )
        self.write_api = self.client.write_api(write_type=SYNCHRONOUS)
        self.query_api = self.client.query_api()
    
    def write_sensor_data(self, dev_eui, data):
        """Write sensor data to InfluxDB
        
        Args:
            dev_eui: Device EUI identifier
            data: Dictionary with sensor readings
        """
        try:
            # Parse timestamp if available, otherwise use current time
            timestamp = data.get('timestamp')
            
            # Build tags
            tags = {
                'dev_eui': dev_eui,
            }
            
            # Build fields (all numeric values)
            fields = {}
            for key, value in data.items():
                if key not in ['dev_eui', 'timestamp']:
                    try:
                        fields[key] = float(value)
                    except (ValueError, TypeError):
                        # Skip non-numeric values
                        pass
            
            if not fields:
                print(f"No valid fields to write for {dev_eui}")
                return
            
            # Build point
            point = {
                'measurement': 'lorawan_data',
                'tags': tags,
                'fields': fields,
            }
            
            if timestamp:
                point['time'] = timestamp
            
            # Write to InfluxDB
            self.write_api.write(
                bucket=settings.INFLUXDB_BUCKET,
                org=settings.INFLUXDB_ORG,
                record=point
            )
            print(f"✓ Data written to InfluxDB: {dev_eui} -> {fields}")
        except Exception as e:
            print(f"✗ Error writing to InfluxDB: {e}")
    
    def get_last_data(self, device_id):
        """Get last data point for a device"""
        try:
            query = f'''
            from(bucket:"{settings.INFLUXDB_BUCKET}")
              |> range(start: -1h)
              |> filter(fn: (r) => r.device_id == "{device_id}")
              |> last()
            '''
            result = self.query_api.query(org=settings.INFLUXDB_ORG, query=query)
            
            data = {}
            for table in result:
                for record in table.records:
                    data[record.field] = record.value
            return data
        except Exception as e:
            print(f"Error querying InfluxDB: {e}")
            return {}
    
    def close(self):
        self.client.close()
