# Manual OAuth Exchange (42 API -> `.oauth_state`)

This guide explains how to manually get 42 OAuth tokens and write `repo/.oauth_state` in the format expected by this project.

Use **Authorization Code flow** (not `client_credentials`), because this project needs a `refresh_token`.

## 1. Prerequisites

Credentials are read from `/srv/42_Network/.env`:

- `CLIENT_ID`
- `CLIENT_SECRET`
- `REDIRECT_URI`
- `SCOPE` (usually `public`)

Load them in your shell:

```bash
set -a
source /srv/42_Network/.env
set +a
```

## 2. Get an authorization code

Open this URL in your browser (replace values):

```text
https://api.intra.42.fr/oauth/authorize?client_id=YOUR_CLIENT_ID&redirect_uri=YOUR_REDIRECT_URI_ENCODED&response_type=code&scope=public&state=RANDOM_STRING
```

After consent, 42 redirects to:

```text
https://your.redirect.uri/callback?code=ABC123...&state=RANDOM_STRING
```

Copy the `code` value.

## 3. Exchange code for tokens

```bash
export CODE="PASTE_CODE_HERE"

curl -sS -X POST "https://api.intra.42.fr/oauth/token" \
  -F grant_type=authorization_code \
  -F client_id="$CLIENT_ID" \
  -F client_secret="$CLIENT_SECRET" \
  -F code="$CODE" \
  -F redirect_uri="$REDIRECT_URI"
```

## 4. Extract fields from response

From the JSON response, manually copy:

- `access_token`
- `refresh_token`

## 5. Write `repo/.oauth_state`

Create/edit `/srv/42_Network/repo/.oauth_state` manually:

```env
ACCESS_TOKEN=PASTE_ACCESS_TOKEN_HERE
REFRESH_TOKEN=PASTE_REFRESH_TOKEN_HERE
EXPIRES_AT=1
```

## 6. Verify token works

```bash
curl -sS -H "Authorization: Bearer PASTE_ACCESS_TOKEN_HERE" https://api.intra.42.fr/v2/me
```

`EXPIRES_AT=1` is intentional: it forces an immediate automatic refresh on first `ensure-fresh`/maintenance cycle.

## Notes

- Recommended automated path remains:
  - `make exchange CODE=...`
  - `make deploy`
