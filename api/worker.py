import os
import django

os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'config.settings')
django.setup()

import time
import pika
import json
from django.conf import settings
from ingestion.influxdb_client import InfluxDBClient

# Single shared InfluxDB client (avoid opening a new connection per message)
_influx_client = None

def get_influx_client():
    global _influx_client
    if _influx_client is None:
        _influx_client = InfluxDBClient()
        print('✓ InfluxDB client initialized')
    return _influx_client

def process_message(ch, method, properties, body):
    """Process message from RabbitMQ and write to InfluxDB"""
    try:
        data = json.loads(body)
        dev_eui = data.get('dev_eui')
        
        if not dev_eui:
            print(f"Missing dev_eui in message: {data}")
            ch.basic_ack(delivery_tag=method.delivery_tag)
            return
        
        # Write to InfluxDB (reuse shared client)
        get_influx_client().write_sensor_data(dev_eui, data)
        
        ch.basic_ack(delivery_tag=method.delivery_tag)
        print(f"✓ Data written to InfluxDB for device {dev_eui}")
    except Exception as e:
        print(f"✗ Error processing message: {e}")
        ch.basic_nack(delivery_tag=method.delivery_tag, requeue=True)

def create_connection():
    """Create a RabbitMQ connection with exponential backoff retry"""
    credentials = pika.PlainCredentials(
        settings.RABBITMQ_USER,
        settings.RABBITMQ_PASSWORD
    )
    params = pika.ConnectionParameters(
        host=settings.RABBITMQ_HOST,
        port=settings.RABBITMQ_PORT,
        virtual_host=settings.RABBITMQ_VHOST,
        credentials=credentials,
        connection_attempts=1,
    )
    max_retries = 10
    delay = 2
    for attempt in range(1, max_retries + 1):
        try:
            print(f'Connecting to RabbitMQ (attempt {attempt}/{max_retries})...')
            connection = pika.BlockingConnection(params)
            print('✓ Connected to RabbitMQ')
            return connection
        except pika.exceptions.AMQPConnectionError as e:
            if attempt == max_retries:
                raise
            print(f'✗ Connection failed: {e}. Retrying in {delay}s...')
            time.sleep(delay)
            delay = min(delay * 2, 30)

def start_consumer():
    """Start RabbitMQ consumer"""
    connection = create_connection()
    channel = connection.channel()
    
    # Declare queue
    channel.queue_declare(queue='lorawan_data', durable=True)
    
    # Set up consumer
    channel.basic_qos(prefetch_count=1)
    channel.basic_consume(
        queue='lorawan_data',
        on_message_callback=process_message
    )
    
    print('🚀 Worker started, waiting for messages...')
    channel.start_consuming()

if __name__ == '__main__':
    try:
        start_consumer()
    except KeyboardInterrupt:
        print('\n🛑 Worker stopped')
