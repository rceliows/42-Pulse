# OAuth 42 Gateway

Petite gateway Node.js pour tester un login OAuth 42 sans toucher au backend principal.

Routes :

- `/auth/login` : redirige vers 42.
- `/auth/callback` : recoit la reponse de 42.
- `/auth/me` : affiche l'utilisateur connecte.
- `/auth/logout` : supprime le cookie.
- le reste : proxy vers `UPSTREAM_URL` si l'utilisateur est connecte.

Variables lues depuis Vault :

- `PUBLIC_BASE_URL`
- `UPSTREAM_URL`
- `SESSION_SECRET`
- `OAUTH_42_CLIENT_ID`
- `OAUTH_42_CLIENT_SECRET`
- `OAUTH_42_SCOPES`
