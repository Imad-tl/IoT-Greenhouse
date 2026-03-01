from django.urls import path, include
from rest_framework.routers import DefaultRouter
from .views import SensorDataViewSet

router = DefaultRouter()
router.register(r'', SensorDataViewSet, basename='sensor_data')

urlpatterns = [
    path('', include(router.urls)),
]
