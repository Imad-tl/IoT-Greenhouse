"""
Generated migration for Device and DeviceMetadata models
"""
from django.conf import settings
from django.db import migrations, models
import django.db.models.deletion


class Migration(migrations.Migration):

    initial = True

    dependencies = [
        migrations.swappable_dependency(settings.AUTH_USER_MODEL),
    ]

    operations = [
        migrations.CreateModel(
            name='Device',
            fields=[
                ('id', models.BigAutoField(auto_created=True, primary_key=True, serialize=False, verbose_name='ID')),
                ('device_id', models.CharField(max_length=100, unique=True)),
                ('name', models.CharField(max_length=255)),
                ('description', models.TextField(blank=True)),
                ('device_type', models.CharField(choices=[('sensor', 'Sensor'), ('gateway', 'Gateway'), ('actuator', 'Actuator')], default='sensor', max_length=20)),
                ('dev_eui', models.CharField(max_length=16, unique=True)),
                ('app_eui', models.CharField(blank=True, max_length=16)),
                ('latitude', models.FloatField(blank=True, null=True)),
                ('longitude', models.FloatField(blank=True, null=True)),
                ('created_at', models.DateTimeField(auto_now_add=True)),
                ('updated_at', models.DateTimeField(auto_now=True)),
                ('is_active', models.BooleanField(default=True)),
                ('owner', models.ForeignKey(on_delete=django.db.models.deletion.CASCADE, to=settings.AUTH_USER_MODEL)),
            ],
            options={
                'ordering': ['-created_at'],
            },
        ),
        migrations.CreateModel(
            name='DeviceMetadata',
            fields=[
                ('id', models.BigAutoField(auto_created=True, primary_key=True, serialize=False, verbose_name='ID')),
                ('fields', models.JSONField(default=dict, help_text='e.g., {"temperature": {"unit": "°C", "type": "float"}}')),
                ('created_at', models.DateTimeField(auto_now_add=True)),
                ('updated_at', models.DateTimeField(auto_now=True)),
                ('device', models.OneToOneField(on_delete=django.db.models.deletion.CASCADE, related_name='metadata', to='devices.device')),
            ],
        ),
    ]
