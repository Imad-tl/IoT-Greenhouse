#!/bin/bash

# Seed the webhook endpoint with sample greenhouse data.
# Uses 2 devices over the last ~5 hours so every Grafana panel has data.
# Usage: ./script/seed.sh  (services must be running)

set -e

API_URL="http://api:8000/api/webhook"

send() {
    docker exec api curl -s -X POST "$API_URL" \
        -H "Content-Type: application/json" \
        -d "$1" | cat
    echo ""
}

echo "Seeding webhook with 15 data points..."

NOW=$(date -u +%s)

# Helper: offset in seconds → ISO-8601 timestamp
ts() { date -u -d "@$(( NOW - $1 ))" +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date -u -r $(( NOW - $1 )) +%Y-%m-%dT%H:%M:%SZ; }

T1=$(ts 18000)  # -5h
T2=$(ts 16200)  # -4h30
T3=$(ts 14400)  # -4h
T4=$(ts 12600)  # -3h30
T5=$(ts 10800)  # -3h
T6=$(ts 9000)   # -2h30
T7=$(ts 7200)   # -2h
T8=$(ts 5400)   # -1h30
T9=$(ts 3600)   # -1h
T10=$(ts 2700)  # -45min
T11=$(ts 1800)  # -30min
T12=$(ts 1200)  # -20min
T13=$(ts 600)   # -10min
T14=$(ts 300)   # -5min
T15=$(ts 60)    # -1min

# Device A — AABBCCDDEE001122
# Device B — FFEEDDCCBBAA9988

echo " 1/15 — Device A, normal"
send '{
  "end_device_ids": {"dev_eui": "AABBCCDDEE001122"},
  "uplink_message": {
    "received_at": "'"$T1"'",
    "rx_metadata": [{"rssi": -72, "snr": 9.5}],
    "decoded_payload": {"seq": 1, "priority": "Normal", "type": "periodic", "temp": 21.3, "hum": 55.0, "water_level": 68}
  }
}'

echo " 2/15 — Device B, normal"
send '{
  "end_device_ids": {"dev_eui": "FFEEDDCCBBAA9988"},
  "uplink_message": {
    "received_at": "'"$T2"'",
    "rx_metadata": [{"rssi": -85, "snr": 7.2}],
    "decoded_payload": {"seq": 1, "priority": "Normal", "type": "periodic", "temp": 19.8, "hum": 62.0, "water_level": 75}
  }
}'

echo " 3/15 — Device A, warming up"
send '{
  "end_device_ids": {"dev_eui": "AABBCCDDEE001122"},
  "uplink_message": {
    "received_at": "'"$T3"'",
    "rx_metadata": [{"rssi": -70, "snr": 10.0}],
    "decoded_payload": {"seq": 2, "priority": "Normal", "type": "periodic", "temp": 23.7, "hum": 52.0, "water_level": 65}
  }
}'

echo " 4/15 — Device B, stable"
send '{
  "end_device_ids": {"dev_eui": "FFEEDDCCBBAA9988"},
  "uplink_message": {
    "received_at": "'"$T4"'",
    "rx_metadata": [{"rssi": -82, "snr": 8.0}],
    "decoded_payload": {"seq": 2, "priority": "Normal", "type": "periodic", "temp": 20.1, "hum": 60.5, "water_level": 72}
  }
}'

echo " 5/15 — Device A, getting hot"
send '{
  "end_device_ids": {"dev_eui": "AABBCCDDEE001122"},
  "uplink_message": {
    "received_at": "'"$T5"'",
    "rx_metadata": [{"rssi": -68, "snr": 10.5}],
    "decoded_payload": {"seq": 3, "priority": "Normal", "type": "periodic", "temp": 27.5, "hum": 48.0, "water_level": 60}
  }
}'

echo " 6/15 — Device B, humid"
send '{
  "end_device_ids": {"dev_eui": "FFEEDDCCBBAA9988"},
  "uplink_message": {
    "received_at": "'"$T6"'",
    "rx_metadata": [{"rssi": -80, "snr": 8.5}],
    "decoded_payload": {"seq": 3, "priority": "Normal", "type": "periodic", "temp": 21.0, "hum": 78.0, "water_level": 70}
  }
}'

echo " 7/15 — Device A, ALERT high temp"
send '{
  "end_device_ids": {"dev_eui": "AABBCCDDEE001122"},
  "uplink_message": {
    "received_at": "'"$T7"'",
    "rx_metadata": [{"rssi": -65, "snr": 11.0}],
    "decoded_payload": {"seq": 4, "priority": "Alert", "type": "alert", "temp": 35.2, "hum": 40.0, "water_level": 52}
  }
}'

echo " 8/15 — Device B, high humidity"
send '{
  "end_device_ids": {"dev_eui": "FFEEDDCCBBAA9988"},
  "uplink_message": {
    "received_at": "'"$T8"'",
    "rx_metadata": [{"rssi": -78, "snr": 9.0}],
    "decoded_payload": {"seq": 4, "priority": "Normal", "type": "periodic", "temp": 22.5, "hum": 85.0, "water_level": 67}
  }
}'

echo " 9/15 — Device A, cooling down"
send '{
  "end_device_ids": {"dev_eui": "AABBCCDDEE001122"},
  "uplink_message": {
    "received_at": "'"$T9"'",
    "rx_metadata": [{"rssi": -71, "snr": 9.8}],
    "decoded_payload": {"seq": 5, "priority": "Normal", "type": "periodic", "temp": 29.0, "hum": 45.0, "water_level": 48}
  }
}'

echo "10/15 — Device B, ALERT low water"
send '{
  "end_device_ids": {"dev_eui": "FFEEDDCCBBAA9988"},
  "uplink_message": {
    "received_at": "'"$T10"'",
    "rx_metadata": [{"rssi": -90, "snr": 6.0}],
    "decoded_payload": {"seq": 5, "priority": "Alert", "type": "alert", "temp": 23.0, "hum": 70.0, "water_level": 12}
  }
}'

echo "11/15 — Device A, stabilizing"
send '{
  "end_device_ids": {"dev_eui": "AABBCCDDEE001122"},
  "uplink_message": {
    "received_at": "'"$T11"'",
    "rx_metadata": [{"rssi": -69, "snr": 10.2}],
    "decoded_payload": {"seq": 6, "priority": "Normal", "type": "periodic", "temp": 25.0, "hum": 50.0, "water_level": 45}
  }
}'

echo "12/15 — Device B, recovering"
send '{
  "end_device_ids": {"dev_eui": "FFEEDDCCBBAA9988"},
  "uplink_message": {
    "received_at": "'"$T12"'",
    "rx_metadata": [{"rssi": -83, "snr": 7.8}],
    "decoded_payload": {"seq": 6, "priority": "Normal", "type": "periodic", "temp": 21.5, "hum": 65.0, "water_level": 30}
  }
}'

echo "13/15 — Device A, ALERT spike"
send '{
  "end_device_ids": {"dev_eui": "AABBCCDDEE001122"},
  "uplink_message": {
    "received_at": "'"$T13"'",
    "rx_metadata": [{"rssi": -66, "snr": 11.5}],
    "decoded_payload": {"seq": 7, "priority": "Alert", "type": "alert", "temp": 42.0, "hum": 35.0, "water_level": 40}
  }
}'

echo "14/15 — Device B, normal"
send '{
  "end_device_ids": {"dev_eui": "FFEEDDCCBBAA9988"},
  "uplink_message": {
    "received_at": "'"$T14"'",
    "rx_metadata": [{"rssi": -81, "snr": 8.2}],
    "decoded_payload": {"seq": 7, "priority": "Normal", "type": "periodic", "temp": 20.5, "hum": 63.0, "water_level": 55}
  }
}'

echo "15/15 — Device A, back to normal"
send '{
  "end_device_ids": {"dev_eui": "AABBCCDDEE001122"},
  "uplink_message": {
    "received_at": "'"$T15"'",
    "rx_metadata": [{"rssi": -70, "snr": 10.0}],
    "decoded_payload": {"seq": 8, "priority": "Normal", "type": "periodic", "temp": 24.0, "hum": 53.0, "water_level": 58}
  }
}'

echo ""
echo "Done! 15 data points sent (2 devices, 3 alerts)."
echo "Open Grafana and set the time range to 'Last 6 hours'."
