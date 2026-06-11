# Refreshing 42 API tokens (repo-local)

## Prereqs
- `/srv/42_Network/.env` contains OAuth vars:
  - `API_ROOT`
  - `CLIENT_ID`
  - `CLIENT_SECRET`
  - `REDIRECT_URI`
  - `SCOPE`
- Run everything from `/srv/42_Network/repo`.

## Steps
1) Get a fresh authorization code in your browser:
   ```text
   https://api.intra.42.fr/oauth/authorize?client_id=YOUR_CLIENT_ID&redirect_uri=YOUR_REDIRECT_URI&response_type=code&scope=public
   ```
   Approve access, then copy `code=...` from the callback URL.

2) Exchange code for tokens:
   ```bash
   cd /srv/42_Network/repo
   make exchange CODE="<AUTHORIZATION_CODE>"
   ```
   This updates `/srv/42_Network/repo/.oauth_state`.

3) Verify token state:
   ```bash
   ../runtime/cache/bin/token-manager-agent token-info
   ```

4) Optional token operations:
   ```bash
   ../runtime/cache/bin/token-manager-agent refresh
   ../runtime/cache/bin/token-manager-agent ensure-fresh
   ../runtime/cache/bin/token-manager-agent call /v2/me
   ```

5) Deploy when tokens are ready:
   ```bash
   make deploy
   ```

## Notes
- `invalid_grant` means the auth code is expired or already used. Generate a new code and exchange immediately.
- If `REDIRECT_URI` changes, update it in both `/srv/42_Network/.env` and your 42 OAuth application settings.
