# Projet S1 — Réseau local 802.15.4 (Thread) : QoS et télémétrie vers le cloud

## 1. Objet

Le présent projet vise à concevoir et valider une architecture IoT complète reposant sur un réseau local basse consommation **802.15.4/Thread**. L’architecture doit permettre la collecte de télémétrie et la remontée d’événements critiques, avec **gestion de priorités (QoS)** au niveau de la passerelle, puis export des données vers le cloud via un uplink **NB-IoT** ou **LoRaWAN**.

## 2. Finalité

Dans un déploiement IoT réel, plusieurs capteurs partagent un même médium radio et produisent des flux hétérogènes :

* **télémétrie périodique** (mesures régulières),
* **alarmes/événements** (flux critiques, non réguliers).

Le projet a pour finalité de démontrer qu’en présence de contraintes (débit, congestion), le système :

1. **préserve les messages critiques**,
2. **dégrade proprement la télémétrie** (compactage, rejet contrôlé),
3. fournit une télémétrie **exploitable** côté cloud (structure, métadonnées, traçabilité).

## 3. Architecture cible

Chaîne fonctionnelle de référence :

**Nœuds capteurs (ESP32-C6, Thread) → Mesh 802.15.4/IPv6 → Passerelle (ESP32-S3) → UART uplink → Cloud**

Remarque d’intégration matériel : lorsque certains capteurs requièrent du **5V**, un microcontrôleur intermédiaire (ex. Arduino) peut être utilisé comme frontend d’alimentation/acquisition avant entrée des données dans le nœud Thread (ESP32-C6), sans modifier la logique réseau (le réseau Thread commence au niveau des C6).

## 4. Exigences fonctionnelles

### 4.1 Réseau Thread

* Mise en place du mesh Thread avec rôles **leader/router/end-device**.
* **Adressage IPv6** et démonstration de la connectivité (idéalement multi-sauts).

### 4.2 Protocole applicatif

* Transport applicatif sur Thread via **UDP** ou **CoAP**.
* Implémentation d’au moins **deux classes de priorité** (ex. `ALARM` vs `TELEMETRY`).

### 4.3 QoS côté passerelle (ESP32-S3)

* Mise en œuvre de **files de messages** (au minimum 2).
* **Ordonnancement** explicite (priorité stricte ou politique hybride).
* Politique de **rejet/compactage** en situation de surcharge.

### 4.4 Uplink UART

* Définition d’un framing stable incluant :

  * longueur et type de message,
  * intégrité (**checksum/CRC** ou MAC),
  * mécanisme **ACK/timeout** (et stratégie associée).

### 4.5 Export cloud

* Remontée des mesures et métadonnées minimales :

  * **timestamp**, **node-id**, et si disponible **RSSI/LQI**.
* Export vers :

  * Variante A : **nRF Cloud** via Thingy:91,
  * Variante B : **TTN** via STM32+LoRa.

### 4.6 Mesures et validation

* Mesure et analyse de :

  * **latence bout-en-bout**,
  * **taux de perte**,
  * **débit utile**,
  * impact des priorités en **régime normal** puis en **surcharge**.

## 5. Livrables attendus

* Codes sources des nœuds Thread (ESP32-C6) et de la passerelle (ESP32-S3).
* Spécification des formats de messages (protocole applicatif + framing UART).
* Résultats expérimentaux (mesures + scénarios) et interprétation.

## 6. Organisation du dépôt

* `ESP32-C6/` : nœuds capteurs Thread
* `ESP32-S3/` : passerelle (QoS + uplink)
* `Arduino-Mega/` : frontend capteurs 5V (si utilisé)
* `docs/` : spécifications protocoles, plan de tests, résultats et figures
