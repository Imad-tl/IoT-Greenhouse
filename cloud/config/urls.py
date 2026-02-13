from django.urls import path, include
from django.http import JsonResponse

def health_check(request):
    """Health check endpoint"""
    return JsonResponse({
        'status': 'healthy',
        'message': 'LoRaWAN Cloud Backend is running ✓'
    })

def hello_world(request):
    """Hello World endpoint"""
    return JsonResponse({
        'message': 'Hello World !',
        'endpoints': {
            'health': '/health',
            'chirpstack_webhook': '/api/webhooks/chirpstack/',
            'ttn_webhook': '/api/webhooks/ttn/'
        }
    })

urlpatterns = [
    path('', hello_world, name='hello_world'),
    path('health', health_check, name='health_check'),
    path('api/webhooks/', include('ingestion.urls')),
]
