# 🧪 Guide de test rapide

## 1. Démarrer les services

```bash
docker-compose up -d

# Vérifier que tous les services sont up
docker-compose ps
```

Expected output:
```
NAME         IMAGE                    STATUS
grafana      grafana/grafana:latest   Up (healthy)
influxdb3    influxdb:3-core         Up (healthy)
rabbitmq     rabbitmq:3-management   Up (healthy)
backend      (build)                 Up (healthy)
worker       (build)                 Up (healthy)
```

## 2. Vérifier les services

```bash
# Backend API
curl http://localhost:8000
# Devrait retourner: 404 (normal, pas de route GET /)

# Logs backend
docker logs backend

# Logs worker
docker logs worker

# RabbitMQ admin
open http://localhost:15672
# User: root / Pwd: root
# Vérifier queue "lorawan_data" vide
```

## 3. Tester un webhook Chirpstack simulé

```bash
# Payload test
curl -X POST http://localhost:8000/api/webhooks/chirpstack/ \
  -H "Content-Type: application/json" \
  -H "X-Signature: test-signature" \
  -d '{
    "deviceInfo": {
      "devEui": "70B3D57ED0041234"
    },
    "rxInfo": {
      "rssi": -80,
      "snr": 10
    },
    "publishedAt": "2024-02-13T10:30:00Z",
    "objectJSON": {
      "temperature": 22.5,
      "humidity": 60,
      "pressure": 1013.25
    }
  }'

# Response expected:
# {"status": "received"}
```

**Note**: La signature n'est pas validée ici (elle sera juste loggée comme invalide mais le message sera quand même publié en dev).

## 4. Vérifier RabbitMQ

```bash
# Ouvrir RabbitMQ admin
open http://localhost:15672
# User: root / Pwd: root

# Vérifier:
# - Queues → "lorawan_data" → 1 message
# - Workers consomme le message
```

## 5. Vérifier InfluxDB

```bash
# Ouvrir InfluxDB
open http://localhost:8181

# Requête pour voir les données:
# SELECT * FROM "lorawan_data"

# Ou via CLI:
docker exec influxdb3-core influxdb3 query "SELECT * FROM lorawan_data"
```

## 6. Vérifier Grafana

```bash
# Ouvrir Grafana
open http://localhost:3000
# User: root / Pwd: root

# Ajouter source de données:
# - URL: http://influxdb3-core:8181
# - Org: lorawan
# - Bucket: lorawan_data
# - Token: test-token

# Créer dashboard simple avec les données
```

## 7. Test TTN webhook

```bash
curl -X POST http://localhost:8000/api/webhooks/ttn/ \
  -H "Content-Type: application/json" \
  -d '{
    "end_device_ids": {
      "dev_eui": "0102030405060708"
    },
    "uplink_message": {
      "decoded_payload": {
        "temperature": 25.5,
        "humidity": 45
      },
      "rx_metadata": [
        {
          "rssi": -75,
          "snr": 8
        }
      ],
      "received_at": "2024-02-13T11:00:00Z"
    }
  }'
```

## 8. Vérifier les logs

```bash
# Backend
docker logs -f backend

# Worker
docker logs -f worker

# RabbitMQ
docker logs rabbitmq

# InfluxDB
docker logs influxdb3-core
```

## 9. Test du flow complet

```bash
# 1. Envoyer webhook
curl -X POST http://localhost:8000/api/webhooks/chirpstack/ \
  -H "Content-Type: application/json" \
  -d '{"deviceInfo":{"devEui":"TEST001"},"rxInfo":{"rssi":-80,"snr":10},"objectJSON":{"temperature":23}}'

# 2. Attendre 2 secondes (traitement worker)
sleep 2

# 3. Vérifier dans InfluxDB
docker exec influxdb3-core influxdb3 query 'SELECT * FROM lorawan_data WHERE dev_eui = "TEST001"'

# 4. Voir dans RabbitMQ → vérifier queue vide
open http://localhost:15672
```

## 🐛 Troubleshooting

### Backend retourne 401
- Signature HMAC invalide? (Normal en test, logs diraient "Invalid signature")
- Vérifier que request body n'est pas vide

### Worker ne traite pas
- Check logs: `docker logs worker`
- Vérifier RabbitMQ est accessible: `docker logs rabbitmq`
- Vérifier InfluxDB token dans `.env`

### InfluxDB vide
- Vérifier worker tourne: `docker ps | grep worker`
- Vérifier logs worker: `docker logs worker`
- Vérifier RabbitMQ a des messages: `http://localhost:15672`

### Grafana ne voit pas les données
- Ajouter datasource InfluxDB correctement
- Bucket: `lorawan_data`
- Token: selon `.env`
- Org: `lorawan`

## 📊 Métriques

Après quelques webhooks:

```sql
-- Nombre de points par device
SELECT COUNT(*) FROM lorawan_data GROUP BY dev_eui

-- Moyenne des températures
SELECT MEAN(temperature) FROM lorawan_data

-- Données des 10 dernières minutes
SELECT * FROM lorawan_data WHERE time > now() - 10m
```

## ✅ Checklist

- [ ] Tous les containers up
- [ ] Backend répond (curl)
- [ ] Worker log "Worker started"
- [ ] Webhook reçoit 200 OK
- [ ] RabbitMQ reçoit message (puis supprime)
- [ ] InfluxDB a les données
- [ ] Grafana affiche les courbes
- [ ] Logs sans erreurs
