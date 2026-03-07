# STM32 LoRaWAN Bridge — ESP32 → TTN

## Vue d'ensemble

Ce module assure la passerelle entre une carte **ESP32-S3** et le réseau **LoRaWAN The Things Network (TTN)**. Il tourne sur une carte **STM32 B-L072Z-LRWAN1** programmée avec **Mbed OS** via Keil Studio Cloud.

```
ESP32-S3  ──(UART)──►  STM32 B-L072Z-LRWAN1  ──(LoRaWAN RF)──►  TTN
```

---

## Rôle de la carte STM32

La STM32 joue le rôle de **pont radio** : elle reçoit des trames texte de l'ESP32 via liaison série, puis les retransmet sur le réseau LoRaWAN vers une application TTN.

Elle ne fait aucun traitement sur les données — elle les transporte.

---

## Connexion physique ESP32 ↔ STM32

La communication entre les deux cartes se fait en **UART 115200 baud** avec le câblage suivant :

| Signal | Broche STM32 | Broche ESP32 |
|--------|-------------|--------------|
| RX STM32 (reçoit) | PA_9 | TX ESP32 |
| TX STM32 (émet) | PA_10 | RX ESP32 |
| GND | GND | GND |

Les trames sont envoyées caractère par caractère et délimitées par un **`\n`** (saut de ligne).

---

## Connexion LoRaWAN — OTAA

La carte rejoint le réseau TTN via la procédure **OTAA (Over The Air Activation)** : à chaque démarrage, elle effectue un **Join Request** au réseau avec ses identifiants, et TTN lui répond un **Join Accept** qui établit la session chiffrée.

Les identifiants (DevEUI, AppEUI, AppKey) sont configurés dans le fichier `mbed_app.json`.

### Paramètres réseau

| Paramètre | Valeur |
|-----------|--------|
| Région | EU868 |
| Activation | OTAA |
| Adaptive Data Rate | Activé |
| Type de message | Non confirmé (unconfirmed uplink) |
| Port applicatif | Défini dans `mbed_app.json` |

---

## Fonctionnement du code

### Démarrage

1. La stack LoRaWAN est initialisée
2. L'ADR (Adaptive Data Rate) est activé — TTN ajuste automatiquement le débit selon la qualité du signal
3. La carte lance le **JOIN** OTAA
4. Une fois connectée, elle commence à sonder l'UART toutes les **200 ms**

### Réception d'une trame ESP32

1. L'UART est lu caractère par caractère jusqu'au `\n`
2. La trame est loggée sur le port série USB (debug PC)
3. Elle est transmise à la fonction d'envoi LoRaWAN

### Envoi LoRaWAN

- La trame est envoyée sur le réseau via `lorawan.send()`
- Si le **duty-cycle** EU868 bloque l'envoi (obligation légale de ne pas émettre en continu), la carte réessaie automatiquement après **3 secondes**
- La taille du payload est limitée à **51 octets** (contrainte LoRaWAN en SF7/EU868)

### Gestion des événements

Tous les événements LoRaWAN sont traités de manière asynchrone via une `EventQueue` Mbed :

| Événement | Action |
|-----------|--------|
| `CONNECTED` | Démarrage de la lecture UART périodique |
| `TX_DONE` | Log de confirmation |
| `TX_ERROR` | Log d'erreur |
| `RX_DONE` | Affichage du downlink reçu (hex) |
| `JOIN_FAILURE` | Log d'erreur — vérifier les clés |
| `DISCONNECTED` | Arrêt du dispatcher |

---

## Debug série

Un port série USB (ST-Link) est disponible à **115200 baud** pour suivre en temps réel l'état de la carte :

```
=== STM32 LoRaWAN Bridge ESP32 -> TTN ===
[LORA] Stack initialisée
[LORA] ADR activé, JOIN en cours...
[LORA] Connecté ! En attente de données ESP32...
[UART] 23 octets reçus, envoi LoRa...
[LORA] 23 octets planifiés
[LORA] Trame envoyée avec succès
```

---

## Point d'attention — DevNonce

TTN v3 exige que le **DevNonce** (identifiant de Join Request) soit strictement croissant entre chaque tentative de JOIN. Comme Mbed OS le stocke en RAM, un reset de la carte repart de zéro et TTN refuse la connexion avec le message :

```
DevNonce is too small
```

**Solution en développement :** activer l'option **"Resets join nonces"** dans les réglages avancés du device sur la console TTN. Cela désactive la vérification stricte du DevNonce.

> Console TTN → Application → Device → General settings → Network layer → Advanced MAC settings → **Resets join nonces** ✅
