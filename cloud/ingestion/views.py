import hmac
import hashlib
from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny
from rest_framework.response import Response
from django.conf import settings
from ingestion.rabbitmq_client import RabbitMQClient

def verify_hmac_signature(request, secret_key):
    """Verify webhook signature using HMAC-SHA256"""
    signature_header = request.META.get('HTTP_X_SIGNATURE', '')
    body = request.body
    
    expected_signature = hmac.new(
        secret_key.encode(),
        body,
        hashlib.sha256
    ).hexdigest()
    
    return hmac.compare_digest(signature_header, expected_signature)

@api_view(['POST'])
@permission_classes([AllowAny])
def chirpstack_webhook(request):
    """
    Chirpstack webhook receiver
    
    Expected headers:
    - X-Signature: HMAC-SHA256 signature
    
    Expected payload:
    {
        "deviceInfo": {"devEui": "0102030405060708"},
        "rxInfo": {"rssi": -80, "snr": 10},
        "objectJSON": {"temperature": 22.5, "humidity": 60}
    }
    """
    try:
        # Verify signature
        if not verify_hmac_signature(request, settings.LORAWAN_WEBHOOK_SECRET):
            return Response(
                {'error': 'Invalid signature'},
                status=status.HTTP_401_UNAUTHORIZED
            )
        
        data = request.data
        dev_eui = data.get('deviceInfo', {}).get('devEui')
        
        if not dev_eui:
            return Response(
                {'error': 'Missing device EUI'},
                status=status.HTTP_400_BAD_REQUEST
            )
        
        # Prepare message for queue
        message = {
            'dev_eui': dev_eui,
            'timestamp': data.get('publishedAt'),
            'rssi': data.get('rxInfo', {}).get('rssi'),
            'snr': data.get('rxInfo', {}).get('snr'),
        }
        
        # Add sensor data from payload
        if 'objectJSON' in data:
            message.update(data['objectJSON'])
        
        # Publish to RabbitMQ
        rabbitmq = RabbitMQClient()
        rabbitmq.publish_message(message)
        rabbitmq.close()
        
        return Response({'status': 'received'}, status=status.HTTP_200_OK)
    
    except Exception as e:
        print(f"Error in webhook: {e}")
        return Response(
            {'error': str(e)},
            status=status.HTTP_500_INTERNAL_SERVER_ERROR
        )

@api_view(['POST'])
@permission_classes([AllowAny])
def ttn_webhook(request):
    """
    TTN webhook receiver
    
    Expected headers:
    - Authorization: Bearer YOUR_API_KEY (optional)
    
    Expected payload:
    {
        "end_device_ids": {"dev_eui": "0102030405060708"},
        "uplink_message": {
            "decoded_payload": {"temperature": 22.5},
            "rx_metadata": [{"rssi": -80, "snr": 10}]
        }
    }
    """
    try:
        data = request.data
        dev_eui = data.get('end_device_ids', {}).get('dev_eui')
        
        if not dev_eui:
            return Response(
                {'error': 'Missing device EUI'},
                status=status.HTTP_400_BAD_REQUEST
            )
        
        # Prepare message
        uplink = data.get('uplink_message', {})
        rx_metadata = uplink.get('rx_metadata', [{}])[0]
        
        message = {
            'dev_eui': dev_eui,
            'timestamp': uplink.get('received_at'),
            'rssi': rx_metadata.get('rssi'),
            'snr': rx_metadata.get('snr'),
        }
        
        # Add decoded payload
        if 'decoded_payload' in uplink:
            message.update(uplink['decoded_payload'])
        
        # Publish to RabbitMQ
        rabbitmq = RabbitMQClient()
        rabbitmq.publish_message(message)
        rabbitmq.close()
        
        return Response({'status': 'received'}, status=status.HTTP_200_OK)
    
    except Exception as e:
        print(f"Error in webhook: {e}")
        return Response(
            {'error': str(e)},
            status=status.HTTP_500_INTERNAL_SERVER_ERROR
        )
