# Plan simple d'integration

## Maintenant

On garde tout dans ce dossier :

- lancer le projet actuel comme d'habitude;
- creer une application OAuth 42;
- remplir `local/oauth.env`;
- lancer `make bootstrap`;
- lancer `make up`;
- tester avec `https://localhost:9443`.

## Plus tard

Quand on pourra modifier le vrai projet :

- fermer l'acces HTTP direct au container `web`;
- garder le WAF comme seule entree publique;
- faire confiance aux headers `X-Auth-*` seulement s'ils viennent de la gateway;
- creer ou retrouver un user local avec `X-Auth-Provider=42` et `X-Auth-Id`;
- ajouter un bouton login/logout dans le frontend.

## Demo rapide

1. Ouvrir `https://localhost:9443`.
2. Montrer que sans login on part vers `/auth/login`.
3. Se connecter avec 42.
4. Ouvrir `/auth/me` pour voir l'utilisateur.
5. Ouvrir `/.env` et montrer que le WAF renvoie `403`.
6. Expliquer que les secrets OAuth sont dans Vault, pas dans Git.
