from django.contrib.admin.apps import AdminConfig

class AdminConfig(AdminConfig):
    default_auto_field = 'django.db.models.BigAutoField'
