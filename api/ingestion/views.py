from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny
from rest_framework.response import Response
from ingestion.rabbitmq_client import RabbitMQClient

@api_view(['POST'])
@permission_classes([AllowAny])
def ttn_webhook(request):
    """
    TTN webhook receiver
    
    Expected headers:
    - Authorization: Bearer <token> (optional, if you want to secure the endpoint)
    
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
        decoded = uplink.get('decoded_payload', {})

        message = {
            'dev_eui':    dev_eui,
            'timestamp':  uplink.get('received_at'),
            'rssi':       rx_metadata.get('rssi'),
            'snr':        rx_metadata.get('snr'),
            # ---- Metadata tags ----
            'seq':      decoded.get('seq'),
            'priority': decoded.get('priority', 'Normal'),
            'type':     decoded.get('type', ''),
            # ---- Sensor fields ----
            'temp':        decoded.get('temp'),
            'hum':         decoded.get('hum'),
            'water_level': decoded.get('water_level'),
        }
        
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
