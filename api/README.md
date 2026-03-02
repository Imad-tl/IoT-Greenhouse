# LoRaWAN Cloud Backend - Simplifié

## 🎯 Architecture Minimaliste

```
┌──────────────────────────────────┐
│   LoRaWAN Network Server         │
│   (TTN)                          │
└────────────┬─────────────────────┘
             │ HTTP Webhooks
             ▼
┌──────────────────────────────────┐
│    Django Backend (Port 8000)    │
│  ✓ RabbitMQ publisher            │
└────────────┬─────────────────────┘
             │
             ▼
       ┌──────────────┐
       │   RabbitMQ   │
       │  (Message    │
       │   Queue)     │
       └──────────────┘
             │
             ▼
┌──────────────────────────────────┐
│    Worker (Message Consumer)     │
│  ✓ Consume from RabbitMQ         │
│  ✓ Write to InfluxDB             │
└────────────┬─────────────────────┘
             │
             ▼
┌──────────────────────────────────┐
│      InfluxDB (Port 8181)        │
│  Time-series database            │
└────────────┬─────────────────────┘
             │
             ▼
┌──────────────────────────────────┐
│    Grafana (Port 3000)           │
│  Dashboards & Visualization      │
└──────────────────────────────────┘
```

## 🚀 Démarrage rapide

### 1. Configuration

```bash
cd backend
cp .env.example .env
```

### 2. Lancer les services

```bash
cd ..
docker-compose up -d
```

### 3. Vérifier les services

```bash
# Backend API
curl http://localhost:8000

# RabbitMQ Admin
http://localhost:15672 (user: root, pwd: root)

# InfluxDB
http://localhost:8181

# Grafana
http://localhost:3000 (user: root, pwd: root)
```

## 📡 Configuration des Webhooks

### TTN

**URL**: `http://your-backend:8000/api/webhooks/ttn/`

Payload attendu:

```json
{
  "end_device_ids": {"dev_eui": "70B3D57ED0041234"},
  "uplink_message": {
    "decoded_payload": {"temperature": 22.5},
    "rx_metadata": [{"rssi": -80, "snr": 10}],
    "received_at": "2024-02-13T10:30:00Z"
  }
}
```

## 🔄 Flow des données

1. Device LoRaWAN → Network Server
2. Network Server → Webhook au Backend
3. Backend normalise payload TTN → publie RabbitMQ
4. Worker consomme RabbitMQ → écrit InfluxDB
5. Grafana lit InfluxDB → affiche données

## 🛠️ Développement

### Logs

```bash
docker logs -f backend
docker logs -f worker
docker logs -f rabbitmq
```

### Arrêter

```bash
docker-compose down
```

## ✨ Points clés

✅ **Pas de PostgreSQL** - InfluxDB seule source de vérité
✅ **Ingestion TTN dédiée** - Endpoint unique côté backend
✅ **Découplage** - RabbitMQ entre ingestion et stockage
✅ **Simple** - Juste webhooks, queue, et InfluxDB
✅ **Scalable** - Worker peut être multiplié
