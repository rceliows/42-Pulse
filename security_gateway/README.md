# security_gateway

Ce dossier est mon ajout pour les modules securite et OAuth.

Important : ce dossier ne modifie pas encore le projet principal. Il est fait a part pour pouvoir le brancher plus tard.

## Idee simple

```text
navigateur -> WAF en HTTPS -> OAuth 42 -> app existante
```

Le WAF sert de porte d'entree securisee.

Vault garde les secrets :

- client id OAuth 42
- client secret OAuth 42
- secret utilise pour signer le cookie de session

La gateway OAuth fait juste ca :

1. si l'utilisateur n'est pas connecte, elle le redirige vers 42;
2. 42 renvoie l'utilisateur sur `/auth/callback`;
3. la gateway cree un cookie de session signe;
4. ensuite elle laisse passer vers l'app.

## Commandes

Creer le fichier local de configuration :

```bash
cp local/oauth.env.example local/oauth.env
```

Remplir `local/oauth.env` avec l'application OAuth creee sur l'intra 42.

Puis lancer :

```bash
make bootstrap
make up
make test
make down
```

URL locale protegee :

```text
https://localhost:9443
```

URL locale de Vault :

```text
https://127.0.0.1:18200
```

## Callback 42

Dans l'application OAuth 42, mettre cette callback :

```text
https://localhost:9443/auth/callback
```

Si on change l'URL publique plus tard, il faudra aussi changer `PUBLIC_BASE_URL` dans `local/oauth.env`.

## Ce que je peux expliquer

- Le WAF bloque les requetes suspectes avant l'application.
- Le WAF bloque aussi les fichiers sensibles comme `/.env`.
- Vault garde les secrets hors de Git.
- OAuth 42 sert a se connecter avec un compte 42.
- La gateway envoie ensuite l'identite au backend avec des headers `X-Auth-*`.

Headers prevus pour plus tard :

- `X-Auth-Provider`
- `X-Auth-Id`
- `X-Auth-Login`
- `X-Auth-Email`
- `X-Auth-Name`

## Limites actuelles

Ce n'est pas encore branche dans le backend principal.

Ce dossier ne modifie pas :

- `infra/docker-compose.yml`
- `app/api/server.js`
- `app/web/nginx.conf`
- le `Makefile` principal
- le schema SQL

Pour rester simple, la gateway proxy les requetes HTTP classiques. Si on veut aussi faire passer les WebSockets par cette gateway, il faudra l'ajouter plus tard.
