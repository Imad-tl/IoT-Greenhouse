# Compte rendu — Partie Cloud

## 1. Vue d'ensemble

La partie cloud du projet IOT Greenhouse constitue l'infrastructure serveur déployée sur un **VPS distant** accessible via le sous-domaine **vps.rayanemerlin.com**. Elle assure la réception, le stockage, la visualisation et l'exposition des données capteurs issues du réseau LoRaWAN. L'ensemble repose sur une architecture de **six conteneurs Docker** orchestrés par Docker Compose, communiquant sur un réseau bridge interne et exposés au monde extérieur par un unique point d'entrée HTTPS.

```
                       ┌──────────────────────────────────────────────────┐
                       │               VPS (vps.rayanemerlin.com)         │
                       │                                                  │
  TTN (webhook)        │   ┌───────┐    ┌─────┐    ┌──────────┐           │
  ─── HTTPS :443 ────► │   │ Nginx │───►│ API │───►│ RabbitMQ │           │
                       │   │       │    └─────┘    └────┬─────┘           │
  Navigateur           │   │       │───► /ui/           │                 │
  ─── HTTPS :443 ────► │   │       │───► /rabbitmq/     ▼                 │
                       │   └───────┘            ┌────────┐                │
                       │        │               │ Worker │                │
                       │        │               └───┬────┘                │
                       │        │                   │                     │
                       │        │               ┌───▼──────┐              │
                       │        │               │ InfluxDB │              │
                       │        │               └───┬──────┘              │
                       │        │                   │                     │
                       │        │     /ui/    ┌─────▼───┐                 │
                       │        └────────────►│ Grafana │                 │
                       │                      └─────────┘                 │
                       └──────────────────────────────────────────────────┘
```

---

## 2. Ingestion des données — API et webhook

### Le point d'entrée : `/api/webhook/`

L'API est un backend **Django** léger, servi par **Gunicorn** (4 workers, timeout 120s). Elle ne possède aucune base de données SQL : tout le stockage repose sur InfluxDB.

Le endpoint principal `POST /api/webhook/` reçoit les **uplinks TTN** (The Things Network). Le payload attendu respecte le format standard TTN :

```json
{
  "end_device_ids": { "dev_eui": "..." },
  "uplink_message": {
    "received_at": "...",
    "decoded_payload": { "temp": 22.5, "hum": 60, "water_level": 80 },
    "rx_metadata": [{ "rssi": -80, "snr": 10 }]
  }
}
```

La vue extrait les champs utiles (identifiant device, timestamp, métadonnées radio, données capteurs) et les structure en un message homogène prêt à être publié.

### Le tampon asynchrone : RabbitMQ

Plutôt que d'écrire directement en base, l'API **publie le message dans une file RabbitMQ** (`lorawan_data`, durable). Ce découplage produit-consommateur apporte trois avantages concrets :

- **Résilience** : si InfluxDB est temporairement indisponible, les messages s'accumulent dans la file sans perte.
- **Réactivité** : l'API répond immédiatement `200 OK` au webhook TTN sans attendre l'écriture en base.
- **Scalabilité** : il est possible de lancer plusieurs workers en parallèle pour absorber des pics de trafic.

### Le consommateur : Worker

Un **conteneur worker** séparé (même image Docker que l'API, mais lançant `python worker.py`) consomme la file RabbitMQ en continu. Pour chaque message, il écrit un point dans InfluxDB via le client dédié. Le système d'acquittement (`basic_ack` / `basic_nack` avec requeue) garantit qu'aucun message n'est perdu en cas d'erreur transitoire. Le worker intègre également une stratégie de **reconnexion avec backoff exponentiel** en cas de perte de connexion à RabbitMQ.

---

## 3. Stockage et requêtage — InfluxDB 3 Core

Les données sont stockées dans **InfluxDB 3 Core**, une base de données temporelle optimisée pour les métriques IoT. Le schéma adopté est le suivant :

| Composant     | Valeur                                              |
|---------------|-----------------------------------------------------|
| Measurement   | `greenhouse_data`                                   |
| Tags          | `dev_eui`, `priority` (Normal/Alert), `type`        |
| Fields        | `seq` (int), `temp` (float), `hum` (float), `water_level` (float) |

Les tags permettent un filtrage rapide par device ou par niveau de priorité, tandis que les fields portent les valeurs numériques indexées temporellement. InfluxDB n'est pas exposé au réseau public : son port `8181` est bindé exclusivement sur `127.0.0.1`, ce qui le rend accessible uniquement aux conteneurs du réseau Docker interne.

---

## 4. Visualisation — Grafana

**Grafana** est provisionné automatiquement au démarrage via des fichiers YAML et JSON embarqués dans le dépôt :

- **Datasource** : connexion pré-configurée vers InfluxDB (requêtage Flux vers `http://influxdb3-core:8181`).
- **Dashboard** : un tableau de bord `iot-dashboard.json` défini comme dashboard d'accueil, affichant les courbes de température, humidité et niveau d'eau en temps réel.

Grafana est configuré pour fonctionner derrière un reverse proxy sur le sous-chemin `/ui/` (option `serve_from_sub_path = true` dans `grafana.ini`). Le thème par défaut est `gloom` (sombre), l'inscription des utilisateurs est désactivée, et l'accès anonyme est interdit.

Voici les cinq panels qui ont été jugés les plus pertinents pour le dashboard :

1. **Température** (Time Series) — Courbe lissée de la température en °C sur la période sélectionnée, avec seuils colorés : vert en dessous de 30 °C, orange entre 30 et 40 °C, rouge au-delà. La légende tabulaire affiche les valeurs last/min/max. Les données sont agrégées par moyenne sur chaque fenêtre temporelle (`aggregateWindow … fn: mean`).

2. **Humidité** (Time Series) — Courbe lissée du taux d'humidité en pourcentage (échelle 0–100 %), avec des seuils similaires : vert en dessous de 80 %, orange entre 80 et 90 %, rouge au-delà. Même agrégation par moyenne que la température.

3. **Niveau d'eau** (Gauge) — Jauge affichant la dernière valeur connue du niveau d'eau (0–100). Les seuils colorent l'indicateur en rouge sous 20, orange entre 20 et 40, et vert au-dessus, permettant de repérer immédiatement un niveau critique.

4. **Alertes** (Stat) — Compteur numérique affichant le nombre total de messages reçus avec le tag `priority = "Alert"` sur la période sélectionnée. Le fond passe au rouge dès qu'au moins une alerte est détectée, avec un mapping textuel : `0 → "Normal"`, `≥ 1 → "ALERTE"`.

5. **Séquence (cpt)** (Time Series, points) — Nuage de points pleine largeur traçant le numéro de séquence (`seq`) de chaque uplink. Ce graphique permet de détecter visuellement les pertes de paquets (trous dans la séquence) ou les doublons.

Le dashboard intègre également une **variable template `device`** qui liste dynamiquement les `dev_eui` présents dans InfluxDB, permettant de filtrer l'ensemble des panels par appareil.

---

## 5. Point d'entrée réseau — Nginx

Nginx joue le rôle de **reverse proxy central et terminaison TLS**. Il constitue l'unique point d'entrée réseau du VPS, exposant les ports 80, 443 et 5671.

### Routage HTTPS (port 443)

| Chemin         | Destination                     | Rôle                          |
|----------------|---------------------------------|-------------------------------|
| `/`            | Fichiers statiques locaux       | Page d'accueil                |
| `/api/`        | `http://api:8000/`              | Backend Django                |
| `/ui/`         | `http://grafana:3000`           | Interface Grafana             |
| `/rabbitmq/`   | `http://rabbitmq:15672/`        | Management UI RabbitMQ        |

Toute requête HTTP (port 80) est redirigée en `301` vers HTTPS, à l'exception du chemin `/.well-known/acme-challenge/` réservé au renouvellement des certificats Let's Encrypt via Certbot.

### Proxy TCP/TLS (port 5671)

Un bloc `stream` Nginx expose le port `5671` en tant que **proxy TCP avec terminaison TLS** vers le port AMQP `5672` de RabbitMQ. Cela permet aux clients AMQP distants (comme les gateways LoRaWAN) de se connecter de façon chiffrée sans que RabbitMQ ait à gérer le TLS lui-même.

### Support WebSocket

Chaque location de proxy transmet les en-têtes `Upgrade` et `Connection` nécessaires au WebSocket, indispensable pour Grafana Live et la management UI de RabbitMQ.

### Gestion des erreurs

Seule la racine exacte (`location = /`) renvoie la page d'accueil. Les fichiers statiques (CSS, etc.) sont servis normalement, mais **toute URL non reconnue retourne une erreur 404** via une page personnalisée (`not-found.html`) utilisant le même thème visuel que l'accueil.

---

## 6. Pages web — Accueil et 404

### Page d'accueil (`/`)

Une page statique servie par Nginx, proposant trois cartes de navigation vers les services :
- **Grafana** (`/ui/`) — dashboards et visualisation
- **API** (`/api/`) — backend Django et webhook
- **RabbitMQ** (`/rabbitmq/`) — management des files de messages

Le design adopte un thème sombre (fond `#0f1117`) avec un accent vert (`#4ade80`) rappelant le contexte greenhouse. Les cartes intègrent des effets de survol (translation verticale, bordure lumineuse).

### Page 404

En cas d'URL inconnue, Nginx sert une page `not-found.html` qui **partage la même feuille de style** que la page d'accueil (`/styles.css`). Elle affiche un code d'erreur typographique proéminent et un lien de retour vers l'accueil, le tout dans une cohérence visuelle avec le reste de l'interface.

---

## 7. Sécurité

### TLS / HTTPS

- Certificats **Let's Encrypt** montés en lecture seule depuis `/etc/letsencrypt` du VPS.
- Protocoles restreints à **TLS 1.2 et 1.3**, ciphers forts (`HIGH:!aNULL:!MD5`), préférence serveur activée.
- Renouvellement automatique via Certbot (volume `certbot-webroot` pour le challenge ACME).

### Variables d'environnement (`.env`)

Tous les secrets sont externalisés dans un fichier `.env` à la racine du projet (non versionné) :
- `DJANGO_SECRET_KEY` — clé secrète Django
- `RABBITMQ_USER` / `RABBITMQ_PASSWORD` — identifiants RabbitMQ
- `GRAFANA_USER` / `GRAFANA_PASSWORD` — identifiants Grafana
- `INFLUXDB_TOKEN` — token d'accès InfluxDB
- `API_KEY` — clé d'API pour la sécurisation éventuelle du webhook
- `DOMAIN` — nom de domaine injecté dynamiquement dans la configuration Nginx via `envsubst`

Aucun secret n'apparaît en dur dans le code ou dans le `docker-compose.yaml` : chaque valeur sensible est lue depuis l'environnement avec un fallback de développement.

### Isolation réseau

- InfluxDB est bindé sur `127.0.0.1:8181` : inaccessible depuis l'extérieur.
- Grafana, l'API et RabbitMQ management ne sont pas exposés directement — ils passent par le reverse proxy Nginx.
- Les conteneurs communiquent sur un réseau Docker bridge interne.
- CORS est restreint à `https://vps.rayanemerlin.com`.

---

## 8. DevOps — Docker Compose

### Conteneurisation

L'ensemble de l'infrastructure est défini dans un unique `docker-compose.yaml` orchestrant **six services** :

| Service      | Image                    | Rôle                                     |
|-------------|--------------------------|------------------------------------------|
| `nginx`     | `nginx:alpine`           | Reverse proxy, terminaison TLS           |
| `api`       | Build depuis `./api`     | Backend Django (Gunicorn)                |
| `worker`    | Build depuis `./api`     | Consommateur RabbitMQ → InfluxDB         |
| `rabbitmq`  | `rabbitmq:3-management`  | Broker de messages AMQP + MQTT           |
| `influxdb3-core` | `influxdb:3-core`   | Base de données temporelle               |
| `grafana`   | `grafana/grafana:latest` | Dashboarding et visualisation            |

### Résilience et orchestration

- **Health checks** : chaque service critique (RabbitMQ, InfluxDB, Grafana, API) déclare un healthcheck. Le worker attend notamment que RabbitMQ et InfluxDB soient sains (`condition: service_healthy`) avant de démarrer.
- **Dépendances** : `depends_on` assure l'ordre de démarrage (InfluxDB → Grafana, RabbitMQ + InfluxDB → Worker, tous → Nginx).
- **Restart policy** : `unless-stopped` sur tous les services pour un redémarrage automatique en cas de crash.
- **Volumes persistants** : les données de Grafana, InfluxDB et RabbitMQ sont stockées dans des volumes Docker nommés, survivant aux redéploiements.

### Workflow de déploiement

Le déploiement se résume à trois étapes :
1. Configurer le `.env` avec les secrets de production
2. S'assurer que les certificats Let's Encrypt sont en place
3. `docker compose up -d`

La configuration Nginx utilise un mécanisme de template (`envsubst`) pour injecter dynamiquement le nom de domaine au démarrage du conteneur, évitant de coder en dur le domaine dans le fichier de configuration.
