# 🗑️ Nettoyage optionnel

## Dossiers qui ne sont plus nécessaires

Les apps Django suivantes ne sont plus utilisées et peuvent être supprimées:

### 1. `backend/devices/`
```bash
rm -rf backend/devices/
```

**Raison**: 
- Models Device/DeviceMetadata ne sont plus utilisés
- API CRUD des devices n'existe plus
- L'identité du device est juste le `dev_eui` (tag InfluxDB)

### 2. `backend/data/`
```bash
rm -rf backend/data/
```

**Raison**:
- Les requêtes de données se font directement via Grafana/InfluxDB
- Pas besoin d'API de query pour le TP
- SensorDataViewSet n'existe plus

## Fichiers à supprimer

```bash
# Migrations Django (pas de BD)
rm -rf backend/devices/migrations/
rm -rf backend/data/migrations/

# Tests inutiles
rm backend/devices/tests.py
rm backend/data/tests.py
rm backend/ingestion/tests.py
```

## Avant de supprimer

✅ Vérifier que `config/settings.py` n'importe pas ces apps:
```python
INSTALLED_APPS = [..., 'ingestion']  # ✅ Pas de 'devices', 'data'
```

✅ Vérifier que `config/urls.py` n'importe pas ces routes:
```python
urlpatterns = [
    path('api/webhooks/', include('ingestion.urls')),  # ✅ Juste ça
]
```

## Structure FINALE minimale

```
backend/
├── config/
├── ingestion/           # ✅ Seule app
├── manage.py
├── worker.py
├── requirements.txt
├── Dockerfile
├── entrypoint.sh
├── .env.example
└── README.md
```

## Commande de nettoyage complète (optionnel)

```bash
# Supprimer les apps inutiles
rm -rf backend/devices backend/data

# Vérifier que tout compile
python backend/manage.py check

# Tester
docker-compose up -d
curl http://localhost:8000
```

## Garder ou Supprimer?

### À GARDER ✅
- `backend/ingestion/` - Webhooks essentiels
- `backend/config/` - Seulement settings, urls, wsgi
- `backend/worker.py` - Consumer RabbitMQ
- Tous les clients (RabbitMQ, InfluxDB)

### À SUPPRIMER ✅
- `backend/devices/` - Plus utilisé
- `backend/data/` - Plus utilisé
- Toutes les migrations Django
- Django admin, auth, etc

### Résultat

**Avant**: ~500 lignes de code Django complexe
**Après**: ~200 lignes simple (webhooks + workers)
