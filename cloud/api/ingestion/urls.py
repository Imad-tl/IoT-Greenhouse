from django.urls import path
from .views import ttn_webhook

urlpatterns = [
    path('', ttn_webhook, name='ttn_webhook'),
]
