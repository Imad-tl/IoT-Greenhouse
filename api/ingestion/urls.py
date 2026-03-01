from django.urls import path
from .views import chirpstack_webhook, ttn_webhook

urlpatterns = [
    path('chirpstack/', chirpstack_webhook, name='chirpstack_webhook'),
    path('ttn/', ttn_webhook, name='ttn_webhook'),
]
