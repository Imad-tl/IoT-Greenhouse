# ESP32-C6 : Nœud capteur (UART → BLE → Passerelle ESP32-S3)

Ce dossier contient le firmware **ESP32-C6** utilisé comme **nœud capteur** dans le projet “Greenhouse”.
Le rôle du C6 est de :
1) recevoir une trame de télémétrie en **UART** (depuis l’Arduino Mega),
2) parser et mettre en forme les données,
3) publier les mesures en **BLE** vers la passerelle **ESP32-S3**.

## 1) Matériel requis

- 1× **ESP32-C6 DevKitC-1** (ou équivalent)
- 1× **Arduino Mega 2560** (frontend capteurs, envoi UART)
- 1× **ESP32-S3** (passerelle BLE central)
- Câble USB pour flasher/monitorer le C6
- Câblage UART + GND commun
- Adaptation de niveau logique (recommandée) :
  - Mega TX = 5V → ESP RX = 3.3V max → diviseur résistif / level shifter

## 2) Câblage

### 2.1 Masse commune (obligatoire)
- **GND Mega ↔ GND ESP32-C6**
- (et GND commun avec la passerelle si nécessaire)

### 2.2 UART (Mega ↔ C6)

#### Pins C6 utilisées
- **UART1 RX = GPIO4**
- **UART1 TX = GPIO5**

#### Pins Mega utilisées
- **TX1 = pin 18**
- **RX1 = pin 19**

#### Connexions recommandées
- **TX1 Mega (18)** → (diviseur 5V→3.3V) → **GPIO4 (RX) C6**
- **GPIO5 (TX) C6** → **RX1 Mega (19)** (optionnel mais utile pour ACK)

> Important : ne branche pas la Mega sur les pins TX/RX “console” de l’ESP (celles liées à l’USB-UART),
> sinon tu crées des conflits avec le flash et le monitor USB.

## 3) Protocole UART (entrée C6)

### 3.1 Framing
- Trames ASCII terminées par fin de ligne : `\n` ou `\r`

### 3.2 Formats attendus (exemples)
- Temp/Hum :
  - `ID=1;T=22.5;H=55`
- Luminosité :
  - `ID=2;LUM=287;ETAT=SOMBRE`
- Mouvement :
  - `ID=3;MOTION=1`

Le C6 parse les champs et met à jour les valeurs publiées en BLE.

### 3.3 ACK
Après parsing d’une trame, le C6 renvoie :
- `ACK\n` sur l’UART

> Note : l’ACK devient réellement utile si l’émetteur (Arduino) implémente timeout/retry + idéalement un SEQ.


## 4) BLE (sortie C6 vers la passerelle)

### 4.1 Rôle BLE
Le C6 est un **périphérique BLE** (GATT Server) :
- il fait de l’advertising sous un nom (ex: `GreenHouse-C6`)
- il expose un service + une characteristic contenant la dernière mesure

### 4.2 Données publiées
La valeur BLE est une string formatée, par exemple :
- `T=22.5;H=55;LUM=287;ETAT=SOMBRE`
- ou selon le nœud : `ID=2;LUM=287;ETAT=SOMBRE`

### 4.3 READ / NOTIFY
- `READ` : la passerelle peut lire la dernière valeur
- `NOTIFY` : à chaque nouvelle trame UART reçue/parsé, le C6 pousse une notification si la passerelle est abonnée

### 4.4 Multi-nœuds (plusieurs C6)
La passerelle S3 (central) scanne, se connecte à plusieurs C6 et s’abonne.
Pour distinguer les sources côté S3 :
- soit par adresse BLE (MAC),
- soit (recommandé) via un champ `ID=` dans la trame.


## 5) Logiciel et build (PlatformIO / ESP-IDF)

### 5.1 Prérequis
- VS Code + extension PlatformIO
ou PlatformIO CLI installé (`pio`)

### 5.2 Commandes (depuis ce dossier)

#### Build
```bash
pio run -e esp32c6
````

#### Flash (upload)
```bash
pio run -e esp32c6 -t upload
```

#### Monitor (logs ESP-IDF)
```bash
pio device list
pio device monitor --port /dev/ttyACM1 --baud 115200
```

> Le port peut être `/dev/ttyACM1`, `/dev/ttyUSB0`, etc. Vérifiez avec `pio device list`.


## 6) Fichiers importants

* `src/main.c` : réception UART, parsing, publication BLE, ACK
* `platformio.ini` : configuration carte, framework, vitesse monitor


## 7) Points de diagnostic (si ça ne marche pas)

### 7.1 Rien n’est reçu sur UART

* vérifier GND commun
* vérifier que l’Arduino envoie sur **Serial1**
* vérifier baudrate des deux côtés (ex 9600)
* vérifier le câblage TX/RX (croisé)
* vérifier adaptation 5V→3.3V sur TX Mega → RX C6

### 7.2 Le flash/monitor USB ne marche pas

* ne pas utiliser les pins TX/RX console pour l’UART Arduino
* vérifier qu’aucun autre monitor n’occupe le port :

  * `lsof /dev/ttyACM1` puis `kill <PID>`

### 7.3 BLE visible mais pas de notifications

* vérifier que la passerelle (ou téléphone) s’abonne bien à la characteristic
* vérifier que `g_connected` est vrai (connexion)
* vérifier qu’une trame UART arrive bien (logs)

## 8) Résumé

* Le C6 reçoit des trames ASCII via UART1 (GPIO4/5),
* parse les champs (T/H/LUM/ETAT/MOTION),
* met à disposition la dernière valeur via BLE (READ),
* pousse des mises à jour via BLE (NOTIFY),
* renvoie `ACK\n` à l’Arduino après traitement.