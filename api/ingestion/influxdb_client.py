from influxdb_client import InfluxDBClient as InfluxDBClientSDK
from influxdb_client.client.write_api import SYNCHRONOUS
from django.conf import settings

# Measurement name used across the project
MEASUREMENT = 'greenhouse_data'

class InfluxDBClient:
    """Client for writing/reading data to/from InfluxDB.

    Schéma InfluxDB :
      measurement : greenhouse_data
      tags        : dev_eui, priority (Normal|Alert), type
      fields      : seq (int), temp (float), hum (float), water_level (float)
    """

    def __init__(self):
        self.client = InfluxDBClientSDK(
            url=settings.INFLUXDB_URL,
            token=settings.INFLUXDB_TOKEN,
            org=settings.INFLUXDB_ORG
        )
        self.write_api = self.client.write_api(write_type=SYNCHRONOUS)
        self.query_api = self.client.query_api()

    def write_sensor_data(self, dev_eui, data):
        """Write one greenhouse sensor record to InfluxDB.

        Args:
            dev_eui : Device EUI (string)
            data    : Dict with keys seq, priority, type, temp, hum, water_level
        """
        try:
            timestamp = data.get('timestamp')

            # --- Tags (categorical) ---
            tags = {
                'dev_eui':   str(dev_eui),
                'priority':  str(data.get('priority', 'Normal')),
                'type':      str(data.get('type', '')),
            }

            # --- Fields (numeric) ---
            fields = {}

            if data.get('seq') is not None:
                try:
                    fields['seq'] = int(data['seq'])
                except (ValueError, TypeError):
                    pass

            for key in ('temp', 'hum', 'water_level'):
                if data.get(key) is not None:
                    try:
                        fields[key] = float(data[key])
                    except (ValueError, TypeError):
                        pass

            if not fields:
                print(f"No valid fields to write for {dev_eui}")
                return

            point = {
                'measurement': MEASUREMENT,
                'tags':   tags,
                'fields': fields,
            }
            if timestamp:
                point['time'] = timestamp

            self.write_api.write(
                bucket=settings.INFLUXDB_BUCKET,
                org=settings.INFLUXDB_ORG,
                record=point
            )
            print(f"✓ InfluxDB write: {dev_eui} | {tags} | {fields}")
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
