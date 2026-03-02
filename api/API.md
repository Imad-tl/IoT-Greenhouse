# API Documentation - LoRaWAN Backend

## Authentication

Tous les endpoints (sauf webhooks) nécessitent une authentification par token.

### Obtenir un token

```bash
curl -X POST http://localhost:8000/api/auth/token/ \
  -H "Content-Type: application/json" \
  -d '{
    "username": "admin",
    "password": "admin"
  }'
```

**Response:**

```json
{
  "token": "9944b09199c62bcf9418ad846dd0e4bbdfc6ee4b"
}
```

### Utiliser le token

Ajouter le header à chaque requête:

```
Authorization: Token YOUR_TOKEN
```

---

## Devices Endpoints

### 1. List devices

```bash
GET /api/devices/
```

**Response:**

```json
[
  {
    "id": 1,
    "device_id": "device_001",
    "name": "Temperature Sensor",
    "description": "Room temperature",
    "device_type": "sensor",
    "dev_eui": "70B3D57ED0041234",
    "app_eui": "",
    "latitude": 48.8566,
    "longitude": 2.3522,
    "owner": 1,
    "created_at": "2024-02-13T10:30:00Z",
    "updated_at": "2024-02-13T10:30:00Z",
    "is_active": true,
    "metadata": {
      "fields": {
        "temperature": {"unit": "°C", "type": "float"},
        "humidity": {"unit": "%", "type": "float"}
      },
      "created_at": "2024-02-13T10:30:00Z",
      "updated_at": "2024-02-13T10:30:00Z"
    }
  }
]
```

### 2. Create device

```bash
POST /api/devices/
```

**Request:**

```json
{
  "device_id": "device_001",
  "name": "Temperature Sensor",
  "description": "Living room temperature",
  "device_type": "sensor",
  "dev_eui": "70B3D57ED0041234",
  "app_eui": "60A7D8E5D0045678",
  "latitude": 48.8566,
  "longitude": 2.3522,
  "is_active": true
}
```

**Response:** Created device object (201)

### 3. Get device

```bash
GET /api/devices/{id}/
```

### 4. Update device

```bash
PATCH /api/devices/{id}/
```

### 5. Delete device

```bash
DELETE /api/devices/{id}/
```

### 6. Set device metadata

```bash
POST /api/devices/{id}/set_metadata/
```

**Request:**

```json
{
  "fields": {
    "temperature": {"unit": "°C", "type": "float", "min": -40, "max": 85},
    "humidity": {"unit": "%", "type": "float", "min": 0, "max": 100},
    "pressure": {"unit": "hPa", "type": "float"}
  }
}
```

### 7. Get last data point

```bash
GET /api/devices/{id}/last_data/
```

**Response:**

```json
{
  "temperature": 22.5,
  "humidity": 60,
  "pressure": 1013.25
}
```

---

## Data Query Endpoints

### 1. Get device data

```bash
GET /api/data/device_data/?device_id=device_001&range=24
```

**Parameters:**

- `device_id` (required): Device identifier
- `range` (optional): Time range in hours (default: 24)

**Response:**

```json
{
  "data": [
    {
      "timestamp": "2024-02-13T10:30:00Z",
      "field": "temperature",
      "value": 22.5,
      "tags": {
        "device_id": "device_001",
        "device_name": "Temperature Sensor",
        "device_type": "sensor"
      }
    }
  ]
}
```

### 2. Get aggregated data

```bash
GET /api/data/aggregation/?device_id=device_001&aggregation=mean&interval=1h&range=24
```

**Parameters:**

- `device_id` (required): Device identifier
- `aggregation` (optional): mean, sum, min, max (default: mean)
- `interval` (optional): Time interval like 1h, 10m, 30s (default: 1h)
- `range` (optional): Time range in hours (default: 24)

**Response:**

```json
{
  "data": [
    {
      "timestamp": "2024-02-13T10:00:00Z",
      "field": "temperature",
      "value": 22.3
    },
    {
      "timestamp": "2024-02-13T11:00:00Z",
      "field": "temperature",
      "value": 22.8
    }
  ]
}
```

---

## Webhooks Endpoints

### TTN Webhook

```bash
POST /api/webhooks/ttn/
```

**Expected payload from TTN:**

```json
{
  "end_device_ids": {
    "device_id": "my-device",
    "dev_eui": "70B3D57ED0041234"
  },
  "uplink_message": {
    "decoded_payload": {
      "temperature": 22.5,
      "humidity": 60
    },
    "rx_metadata": [
      {
        "rssi": -80,
        "snr": 10,
        "uplink_token": "CkEQ..."
      }
    ],
    "received_at": "2024-02-13T10:30:00Z"
  }
}
```

---

## Error Responses

### 400 Bad Request

```json
{
  "error": "device_id parameter required"
}
```

### 401 Unauthorized

```json
{
  "detail": "Authentication credentials were not provided."
}
```

### 404 Not Found

```json
{
  "error": "Device not found"
}
```

### 500 Internal Server Error

```json
{
  "error": "Error message"
}
```

---

## Example Workflow

### 1. Authenticate

```bash
TOKEN=$(curl -s -X POST http://localhost:8000/api/auth/token/ \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin"}' | jq -r '.token')
```

### 2. Create device

```bash
curl -X POST http://localhost:8000/api/devices/ \
  -H "Authorization: Token $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "temp_sensor_01",
    "name": "Living Room Sensor",
    "dev_eui": "70B3D57ED0041234",
    "device_type": "sensor"
  }'
```

### 3. Query recent data

```bash
curl -X GET "http://localhost:8000/api/data/device_data/?device_id=temp_sensor_01&range=24" \
  -H "Authorization: Token $TOKEN"
```

### 4. Get hourly averages

```bash
curl -X GET "http://localhost:8000/api/data/aggregation/?device_id=temp_sensor_01&aggregation=mean&interval=1h" \
  -H "Authorization: Token $TOKEN"
```
