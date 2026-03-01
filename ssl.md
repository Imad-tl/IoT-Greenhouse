# Commande pour générer un certificat SSL avec certbot en mode standalone

```markdown
certbot certonly --standalone -d vps.rayanemerlin.com --non-interactive --agree-tos --register-unsafely-without-email
```

Cette commande utilise certbot pour générer un certificat SSL pour le domaine `vps.rayanemerlin.com` en mode standalone. Les options utilisées sont les suivantes :
- `certonly` : Indique que nous voulons uniquement obtenir le certificat sans configurer automatiquement le serveur web.
- `--standalone` : Utilise le mode standalone, ce qui signifie que certbot va temporairement démarrer un serveur web pour valider le domaine.
- `-d vps.rayanemerlin.com` : Spécifie le domaine pour lequel le certificat SSL doit être généré.
- `--non-interactive` : Permet d'exécuter la commande sans interaction, ce qui est utile pour les scripts automatisés.
- `--agree-tos` : Accepte les termes de service de Let's Encrypt.
- `--register-unsafely-without-email` : Permet de s'enregistrer sans fournir d'adresse e-mail, ce qui est généralement déconseillé car cela empêche de recevoir des notifications importantes concernant le certificat.


Penser à bien mettre les bons ports ouverts (80 et 443) sur votre serveur pour que certbot puisse valider le domaine et générer le certificat SSL correctement. Penser aussi a mettre les bons certificats dans votre configuration nginx pour que le SSL fonctionne correctement.