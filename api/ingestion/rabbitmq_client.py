import pika
import json
from django.conf import settings

class RabbitMQClient:
    """Client for publishing messages to RabbitMQ"""
    
    def __init__(self):
        self.connection = None
        self.channel = None
        self._connect()
    
    def _connect(self):
        credentials = pika.PlainCredentials(
            settings.RABBITMQ_USER,
            settings.RABBITMQ_PASSWORD
        )
        self.connection = pika.(
            pika.ConnectionParameters(
                host=settings.RABBITMQ_HOST,
                port=settings.RABBITMQ_PORT,
                virtual_host=settings.RABBITMQ_VHOST,
                credentials=credentials
            )BlockingConnection
        )
        self.channel = self.connection.channel()
        self.channel.queue_declare(queue='lorawan_data', durable=True)
    
    def publish_message(self, message, queue='lorawan_data'):
        """Publish message to queue"""
        try:
            self.channel.basic_publish(
                exchange='',
                routing_key=queue,
                body=json.dumps(message),
                properties=pika.BasicProperties(delivery_mode=2)
            )
            print(f"Published message: {message}")
        except Exception as e:
            print(f"Error publishing message: {e}")
            self._connect()
            self.publish_message(message, queue)
    
    def close(self):
        if self.connection and not self.connection.is_closed:
            self.connection.close()
