# Arduino Mega (Frontend capteurs)

Ce dossier contient le firmware **Arduino Mega 2560** utilisé comme **frontend capteurs** dans le projet “Greenhouse”.
Le rôle du Mega est de :
1) alimenter les capteurs (5V),
2) lire les mesures,
3) transmettre une trame de télémétrie vers un nœud **ESP32-C6** via UART.

---

## 1) Matériel requis

- 1× **Arduino Mega 2560**
- 1× **ESP32-C6** (nœud capteur correspondant)
- Capteurs :
  - Température/Humidité (DHT11)
  - Luminosité (photorésistance)
  - Mouvement (PIR)

- Fils Dupont + breadboard
- Adaptation de niveau logique Mega→ESP (recommandée) :
  - diviseur résistif ou level shifter (Mega = 5V, ESP = 3.3V)

---

## 2) Architecture (vue côté Mega)

Capteurs → **Mega** (lecture) → **Serial1** (TX1/RX1) → ESP32-C6 → (BLE vers ESP32-S3)

Le Mega n’est pas un nœud réseau : il fournit les mesures (du capteur température/humidité) au C6, qui les publie ensuite.

---

## 3) Câblage

### 3.1 Masse commune (obligatoire)
- **GND Mega ↔ GND ESP32-C6**
- GND des capteurs relié au GND Mega

### 3.2 Lien UART Mega → ESP32-C6 (Serial1)
Sur Mega 2560 :
- **TX1 = pin 18**
- **RX1 = pin 19**

Branchement recommandé :
- **TX1 Mega (18) → (diviseur 5V→3.3V) → RX ESP32-C6 (GPIO4)**  
- **TX ESP32-C6 (GPIO5) → RX1 Mega (19)** (optionnel mais utile si on veut gérer ACK)

> Important : le Mega sort du 5V sur TX. Les GPIO ESP32 sont 3.3V max.  
> Un diviseur résistif simple suffit (ex: R1=2k en série, R2=3.3k vers GND côté ESP).

### 3.3 Capteurs (exemples)
Les pins exactes peuvent varier selon votre repo et votre montage. Exemple typique :
- DHT : DATA sur pin **2**
- LDR : analog sur **A0**
- PIR : digital sur un pin (ex: 7)

---

## 4) Format des trames envoyées

Le Mega envoie des trames **ASCII** simples, finies par retour à la ligne `\n`.

Exemples :
- Temp/Hum :
  - `T=22.5;H=55`
- Luminosité :
  - `LUM=287;ETAT=SOMBRE`
- Mouvement :
  - `MOTION=1`

Ces formats ont été choisis pour être :
- lisibles au moniteur série,
- faciles à parser côté ESP32-C6 (recherche de tags `T=`, `H=`, etc.),
- utilisables en notifications BLE après publication.

> Selon la version du code, un champ `ID=` peut être ajouté pour identifier la source :
> `ID=1;T=22.5;H=55`

---

## 5) Logiciel et build (PlatformIO)

Cette partie est gérée via **PlatformIO**.

### 5.1 Prérequis
- VS Code + extension PlatformIO  
ou PlatformIO CLI installé

### 5.2 Commandes utiles
Se placer dans le dossier `Arduino-Mega/` :

#### Build
```bash
pio run -e mega2560
```

#### Flash (upload)

```bash
pio run -e mega2560 -t upload
```

#### Monitor (USB Mega)

```bash
pio device monitor --port /dev/ttyACM0 --baud 9600
```

#### Trouver le port série

```bash
pio device list
```

---

## 6) Points de diagnostic (si ça ne marche pas)

### 6.1 Upload échoue (avrdude / sync)

* Vérifier que la carte est bien une **Mega 2560**
* Vérifier le port avec `pio device list`
* Fermer tout autre moniteur série (port occupé)

### 6.2 Rien n’arrive côté ESP32-C6

* Vérifier **GND commun**
* Vérifier que le Mega envoie sur **Serial1** (pas Serial)
* Vérifier le baudrate :

  * Mega `Serial1.begin(...)` doit matcher le C6 (ex: 9600 ou 115200)
* Vérifier l’adaptation de niveau (Mega TX1 → ESP RX)

### 6.3 Valeurs incohérentes

* Vérifier le câblage du capteur (VCC/GND/DATA)
* Pour DHT : vérifier que le capteur est bien initialisé et pas “NaN”
* Pour LDR : vérifier le pont diviseur (si utilisé) et le seuil

---

## 7) Ce que fait le Mega dans le projet (résumé)

* Lit les capteurs à une cadence fixe.
* Génère une trame texte standardisée.
* Envoie la trame sur **Serial1** vers l’ESP32-C6.
* (Optionnel) peut recevoir un `ACK` renvoyé par l’ESP32-C6.

---

## 8) Notes

* Les sorties UART du Mega (TX1) sont en 5V logique : protéger l’ESP32.
* Le Mega garde `Serial` (USB) pour debug, et utilise `Serial1` pour l’envoi vers ESP.



