# Justification des modules

## Cybersecurity

Ce dossier ajoute :

- un WAF ModSecurity/OWASP CRS;
- HTTPS devant l'app;
- Vault pour garder les secrets;
- des regles WAF contre les fichiers sensibles;
- des limites simples contre trop de requetes.

## OAuth 2.0

J'ai garde uniquement OAuth 42 pour rester simple.

Le flow :

```text
/auth/login -> page 42 -> /auth/callback -> cookie de session -> app
```

La gateway prepare l'integration future avec ces headers :

- `X-Auth-Provider`
- `X-Auth-Id`
- `X-Auth-Login`
- `X-Auth-Email`
- `X-Auth-Name`

Comme ca, plus tard, le backend pourra relier le compte 42 a un utilisateur local.
