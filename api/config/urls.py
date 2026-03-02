from django.urls import path, include
from django.http import JsonResponse

def health_check(request):
    """Health check endpoint"""
    return JsonResponse({
        'status': 'healthy',
        'message': 'LoRaWAN Cloud Backend is running ✓'
    })

def welcome(request):
    return JsonResponse({
        'message': 'Welcome to the IOT Greenhouse API Gateway! 🌱',
        'endpoints': [
            {
                "name": "health",
                "method": "GET",
                "path": "/api/health",
                "description": "Check if the API is running"
            },
            {
                "name": "webhook",
                "method": "POST",
                "path": "/api/webhook",
                "description": "Receive data from The Things Network (TTN) via webhook"
            }
        ]
    })

urlpatterns = [
    path('', welcome, name='welcome'),
    path('health', health_check, name='health_check'),
    path('webhook/', include('ingestion.urls')),
]
