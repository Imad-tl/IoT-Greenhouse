# ESP32-S3 : Passerelle (BLE Central → Déchiffrement AES → UART STM32)

Ce dossier contient le firmware **ESP32-S3** utilisé comme **passerelle (Gateway)** dans le projet "Greenhouse".
Le rôle du S3 est de :
1) scanner et se connecter aux **nœuds BLE** (ESP32-C6),
2) recevoir, **déchiffrer (AES-128-CBC)** et agréger les trames de télémétrie,
3) transmettre les données vers le module **STM32/LoRa** via **UART**.

---

## 1) Matériel requis

- 1× **ESP32-S3 DevKitC-1** (ou équivalent)
- 1–3× **ESP32-C6** (nœuds capteurs BLE périphériques)
- 1× **STM32 + module LoRa** (uplink LoRaWAN, réception UART)
- Câble USB pour flasher/monitorer le S3
- Câblage UART + GND commun entre S3 et STM32

---

## 2) Câblage

### 2.1 Masse commune (obligatoire)
- **GND S3 ↔ GND STM32**

### 2.2 UART vers STM32

#### Pins S3 utilisées
- **UART1 TX = GPIO43**
- **UART1 RX = GPIO44**

#### Connexions
- **GPIO43 (TX) S3** → **RX STM32**
- **GPIO44 (RX) S3** → **TX STM32**

> Important : ces broches sont dédiées à la liaison STM32.
> Le port USB du S3 reste indépendant et disponible pour le flash et le monitor.

---

## 3) Architecture logicielle

Le firmware s'articule autour de trois blocs principaux qui s'enchaînent à chaque réception BLE.

```
BLE NOTIFY reçue
      │
      ▼
decrypt_and_process()   ← déchiffrement AES-128-CBC + retrait padding PKCS7
      │
      ▼
  Identification ID=1 / ID=2 / ID=3
      │
      ▼
  Mise à jour nodes[]   ← structure d'agrégation centralisée
      │
      ▼
envoyer_vers_stm32_fiable()  ← UART + attente ACK;SEQ=n
```

---

## 4) Protocole BLE (entrée S3)

### 4.1 Rôle BLE
Le S3 est un **central BLE** (GATT Client) :
- il scanne en continu l'environnement,
- il se connecte à tout périphérique dont le nom commence par `GreenHouse-C6-`,
- il négocie le MTU (256 octets), découvre le service `0x00FF` et la caractéristique `0xFF01`,
- il s'abonne aux notifications via l'écriture du CCCD.

### 4.2 Séquence de connexion automatique
À chaque connexion, le S3 enchaîne automatiquement :
1. Négociation MTU → 256 octets
2. Découverte du service `0x00FF`
3. Découverte de la caractéristique `0xFF01`
4. Écriture CCCD `0x0100` → activation des notifications

En cas de déconnexion, le scan est relancé immédiatement.

### 4.3 Format des trames reçues (après déchiffrement)
```text
ID=1;SEQ=42;T=22.5;H=55         ← Capteur température/humidité
ID=2;SEQ=17;LUM=287;ETAT=SOMBRE ← Capteur luminosité
ID=3;SEQ=8;MOTION=1             ← Capteur mouvement
```

---

## 5) Déchiffrement AES-128-CBC

### 5.1 Principe
Les payloads reçues en BLE sont chiffrées côté C6. La fonction `decrypt_and_process()` réalise le déchiffrement :

- **Octets 0–15** : Vecteur d'initialisation (IV)
- **Octets 16–N** : Ciphertext chiffré avec AES-128-CBC
- La **PSK (Pre-Shared Key)** de 128 bits est partagée entre tous les nœuds et la passerelle

```c
// Clé PSK (à ne pas laisser en clair dans un déploiement réel)
uint8_t PSK[16] = { 0x53, 0x65, 0x72, 0x72, 0x65, ... };
```

### 5.2 Gestion du padding PKCS7
Après déchiffrement, le dernier octet indique le nombre d'octets de padding à retirer.
Si la valeur est invalide (> 16 ou = 0), la trame est **rejetée** et un log d'erreur est généré.

### 5.3 Fallback texte clair
Si la payload fait **moins de 32 octets**, elle est traitée comme du texte clair (mode test).
Un log `ESP_LOGW` signale ce cas.

> Remarque : tout rejet de trame (padding invalide, erreur mbedtls) génère un `ESP_LOGE`.
> Ces logs constituent la journalisation des événements de sécurité.

---

## 6) Protocole UART (sortie S3 vers STM32)

### 6.1 Configuration
- **Port** : `UART_NUM_1`
- **Baudrate** : 115200
- **TX** : GPIO43 — **RX** : GPIO44

### 6.2 Format de trame envoyée
```text
S1;SEQ=<n>;PRIO=<H|N>;TYPE=TEL;T=<val>;H=<val>;LUM=<val>;MOT=<0|1>\n
```

- `PRIO=H` (High) : si `MOTION=1` → événement critique, remontée prioritaire
- `PRIO=N` (Normal) : télémétrie standard
- `SEQ` : compteur monotone incrémenté à chaque envoi **confirmé**

### 6.3 Mécanisme d'acquittement
Après chaque envoi, le S3 attend la réponse du STM32 pendant **2 secondes** :

```text
S3  → STM32 : "S1;SEQ=5;PRIO=N;TYPE=TEL;T=22.5;H=55;LUM=SOMBRE;MOT=0\n"
STM32 → S3  : "ACK;SEQ=5"
```

- Si l'ACK est validé → `g_seq` est incrémenté.
- Si timeout ou ACK incorrect → log `ESP_LOGE`, la trame n'est **pas** recomptée.

> Note : le buffer UART est vidé (`uart_flush`) avant chaque envoi pour éviter de lire un vieil ACK résiduel.

---

## 7) Agrégation multi-nœuds

Le S3 maintient un tableau de 3 structures `node_data_t` :

```c
typedef struct {
    char last_data[128];  // Dernière trame déchiffrée reçue
    bool connected;
} node_data_t;

static node_data_t nodes[MAX_NODES]; // MAX_NODES = 3
```

Le champ `ID=` de chaque trame déchiffrée détermine l'index de mise à jour :
- `ID=1` → `nodes[0]` (température/humidité)
- `ID=2` → `nodes[1]` (luminosité)
- `ID=3` → `nodes[2]` (mouvement)

---

## 8) Tâche de monitoring

Une tâche FreeRTOS `monitor_task` s'exécute en arrière-plan et affiche toutes les **10 secondes** un résumé sur la console série :

```
--- RÉSUMÉ CAPTEURS ---
Node 1: ID=1;SEQ=42;T=22.5;H=55
Node 2: ID=2;SEQ=17;LUM=287;ETAT=SOMBRE
Node 3: ---
```

`---` indique qu'aucune donnée n'a encore été reçue de ce nœud (non connecté ou perte BLE).

---

## 9) Logiciel et build (ESP-IDF)

### 9.1 Prérequis
- Environnement **EIM** (ESP-IDF) installé et sourcé
- Composants requis : `nimble`, `mbedtls` (inclus dans ESP-IDF)

### 9.2 Commandes (depuis ce dossier)

#### Build
```bash
idf.py build
```

#### Flash
```bash
idf.py flash
```

#### Monitor
```bash
idf.py monitor
```

> Les trois commandes peuvent être chaînées en une seule :
> ```bash
> idf.py build flash monitor
> ```

---

## 10) Fichiers importants

- `src/gateway_s3.c` : scan BLE, déchiffrement AES, agrégation, envoi UART, monitoring
- `platformio.ini` : configuration carte, framework, vitesse monitor

---

## 11) Points de diagnostic (si ça ne marche pas)

### 11.1 Aucun nœud C6 détecté en BLE

- Vérifier que le(s) C6 sont bien allumés et en advertising
- Vérifier que le nom annoncé commence bien par `GreenHouse-C6-` (avec le tiret final)
- Vérifier qu'aucun autre central n'est déjà connecté au C6 (connexion exclusive)

### 11.2 Notifications reçues mais déchiffrement échoue

- Vérifier que la **PSK est identique** côté C6 et côté S3
- Vérifier que l'**IV** (16 premiers octets) est bien inclus dans la payload envoyée par le C6
- Vérifier que la payload fait bien **un multiple de 16 octets** (contrainte AES-CBC)
- Vérifier que le MTU négocié est suffisant (≥ 32 octets) :
  - `lsof /dev/ttyACM0` puis `kill <PID>` si le monitor bloque le port

### 11.3 STM32 ne reçoit pas les trames UART

- Vérifier le **GND commun** S3 ↔ STM32
- Vérifier le **croisement TX/RX** (TX S3 → RX STM32)
- Vérifier que le baudrate est **115200** des deux côtés
- Vérifier que le STM32 renvoie bien `ACK;SEQ=<n>` dans les 2 secondes

### 11.4 Le flash/monitor USB ne marche pas

- Vérifier qu'aucun autre process n'occupe le port série :
  ```bash
  lsof /dev/ttyACM0
  kill <PID>
  ```
- S'assurer que l'environnement EIM est bien sourcé avant de lancer `idf.py`

---

## 12) Résumé

- Le S3 scanne en BLE et se connecte automatiquement aux nœuds `GreenHouse-C6-X`,
- négocie le MTU, découvre le service GATT et active les notifications,
- déchiffre chaque payload AES-128-CBC (IV + ciphertext + PKCS7),
- agrège les données dans un tableau indexé par `ID=`,
- construit une trame priorisée (`PRIO=H/N`) et l'envoie au STM32 via UART avec ACK,
- affiche un résumé de l'état de tous les nœuds toutes les 10 secondes.
