# IoT Greenhouse — Linux only

## Prérequis

- Linux (toute distribution parmi : Debian/Ubuntu, RHEL/CentOS, Fedora, Arch, openSUSE, Alpine, Snap)
- `make` — se bootstrape via `bash scripts/install-make.sh` si absent

---

## Pourquoi Make ?

Ce projet couvre trois domaines distincts :

| Domaine | Langage | Compilation |
|---|---|---|
| Firmware IoT (LoRaWAN) | C / C++ | **Oui** — cross-compilation ESP-IDF, linkage, flash |
| Intégration TTN | C | **Oui** (à venir) |
| Infrastructure cloud | Bash / Docker | Non — orchestration de services |

Le code firmware est du **C compilé** — `make` est donc une dépendance incontournable sur toute machine qui touche au projet.
Ce choix étant déjà imposé, il était naturel d'unifier la surface d'entrée du projet entier sous le même outil plutôt que d'introduire un second task runner (npm scripts, just, taskfile…) pour le cloud.
Make devient ainsi le **point d'entrée unique** : un développeur clone le dépôt, tape `make <cible>` et tout fonctionne, qu'il travaille sur le firmware ou sur l'infrastructure.

---

## Architecture des Makefiles

```
Makefile              ← point d'entrée racine, include les modules
make/
  cloud.mk            ← cibles cloud-*
  lorawan.mk          ← cibles lorawan-* (compilation firmware, à venir)
  ttn.mk              ← cibles ttn-* (intégration TTN, à venir)
```

### Principe de modularisation

Le Makefile racine ne contient aucune recette métier : il délègue à des modules thématiques via `include`.

```makefile
# Makefile
include make/cloud.mk
include make/lorawan.mk
include make/ttn.mk
```

Chaque module est **autonome et préfixé** : toutes les cibles d'un domaine partagent le même préfixe (`cloud-*`, `lorawan-*`, `ttn-*`).
Cela évite les collisions entre domaines et rend la cible appelée auto-documentée (`make cloud-start` vs `make lorawan-flash`).

### Dispatch générique par pattern rule

Chaque module exploite la règle de pattern Make `%` pour éviter de déclarer une cible par script :

```makefile
# make/cloud.mk
cloud-%:
    bash $(CLOUD_SCRIPTS_DIR)/$*.sh
```

`make cloud-start` → `bash scripts/cloud/start.sh`
`make cloud-cert`  → `bash scripts/cloud/cert.sh`
`make cloud-seed`  → `bash scripts/cloud/seed.sh`

Ajouter un nouveau script `scripts/cloud/foo.sh` expose automatiquement `make cloud-foo` sans aucune modification du Makefile.

Les seules **surcharges explicites** sont les cibles sans script 1-pour-1 :

```makefile
cloud-stop:     # docker compose down — pas un script
cloud-daemon:   # start.sh --daemon  — variante avec flag
cloud-restart:  # dépendance cloud-stop + cloud-start
```

### "Accès privé" par l'absence

Les alias courts (`start`, `stop`, `cert`…) ne sont délibérément **pas déclarés** dans le Makefile racine.
`make start` retourne `No rule to make target` — seul `make cloud-start` est accessible.
C'est la façon idiomatique en Make d'encapsuler les détails d'implémentation d'un module.

---

## Cibles disponibles

### Cloud (`make/cloud.mk`)

| Cible | Action |
|---|---|
| `make cloud-start` | Démarre la stack Docker Compose (mode attaché) |
| `make cloud-daemon` | Démarre la stack en arrière-plan (`-d`) |
| `make cloud-stop` | Arrête et supprime les containers |
| `make cloud-restart` | `cloud-stop` puis `cloud-start` |
| `make cloud-cert` | Génère / renouvelle le certificat SSL Let's Encrypt |
| `make cloud-seed` | Peuple l'API avec 15 points de données de test |

### LoRaWAN (`make/lorawan.mk`) — à venir

Compilation du firmware C, flash, monitoring série.

### TTN (`make/ttn.mk`) — à venir

Provisioning The Things Network, enregistrement des devices.

---

## Structure des scripts

```
scripts/
  install-make.sh       ← bootstrap : installe make (source lib/lib.sh)
  lib/
    lib.sh              ← fonctions universelles : detect_pkg_manager,
                           install_package, load_env, require_pkg_manager
  cloud/
    _commons.sh         ← fonctions cloud + bootstrap (source lib.sh,
                           load .env, require_pkg_manager)
    start.sh            ← installe Docker si absent, lance compose
    cert.sh             ← installe certbot si absent, génère le cert SSL
    seed.sh             ← envoie des données de test à l'API webhook
```

### Chaîne de sourcing

```
start.sh / cert.sh
  └── source _commons.sh
        └── source lib/lib.sh
```

Sourcer `_commons.sh` suffit : cela charge `lib.sh`, initialise les variables
(`PROJECT_DIR`, `CLOUD_ENV_FILE`), charge le `.env` et détecte le gestionnaire
de paquets. Aucun script cloud ne répète ces initialisations.

### Support multi-distribution

`lib.sh` supporte nativement : `apt-get`, `yum`, `dnf`, `pacman`, `zypper`, `apk`, `snap`.
La fonction `install_package <paquet>` abstrait l'appel au gestionnaire détecté.
Les fonctions spécifiques cloud (`install_docker`, `install_certbot`, `resolve_compose_cmd`)
n'ont des cas explicites que pour les distributions nécessitant une configuration de dépôt
ou des noms de paquets non-standards ; les autres passent par `install_package`.

---

## Démarrage rapide

```bash
# 1. Cloner et aller à la racine
git clone https://github.com/Imad-tl/IoT-Greenhouse.git
cd IoT-Greenhouse

# 2. Installer make si nécessaire
bash scripts/install-make.sh

# 3. Créer le fichier .env à partir du template
cp cloud/.env.template cloud/.env
# éditer cloud/.env (DOMAIN, mots de passe…)

# 4. Générer le certificat SSL
make cloud-cert

# 5. Démarrer la stack
make cloud-daemon

# 6. (Optionnel) Peupler avec des données de test
make cloud-seed
``` 
