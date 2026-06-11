# 42 Network Deployment Guide

This guide is aligned with the current repository layout and scripts in `repo/`.

## 1. Prerequisites

Required on host:

```bash
docker
docker compose
bash
curl
jq
python3
```

Recommended install (Ubuntu/Debian):

```bash
sudo apt update
sudo apt install -y docker.io docker-compose-plugin curl jq python3
```

## 2. Configure Environment

From `/srv/42_Network`:

```bash
cp repo/.env.example .env
```

Edit `.env` and set:
- `CLIENT_ID`
- `CLIENT_SECRET`
- `REDIRECT_URI`
- `SCOPE` (usually `public`)
- `REFRESH_TOKEN`
- DB values if you do not want defaults
- `WEB_PORT` (optional)

Important:
- `repo/.cache/bin/toolkit-agent check_environment` validates `REFRESH_TOKEN` in `/srv/42_Network/.env`.
- Runtime token state is stored in `repo/.oauth_state`.

## 3. First OAuth Exchange

If `repo/.oauth_state` does not exist, perform a one-time authorization code exchange.

### 3.1 Build authorize URL

```bash
cd /srv/42_Network
source .env
python3 - <<'PY'
import os, urllib.parse
api_root = os.getenv("API_ROOT", "https://api.intra.42.fr")
client_id = os.environ["CLIENT_ID"]
redirect_uri = urllib.parse.quote(os.environ["REDIRECT_URI"], safe="")
scope = os.environ.get("SCOPE", "public")
print(f"{api_root}/oauth/authorize?client_id={client_id}&redirect_uri={redirect_uri}&response_type=code&scope={scope}")
PY
```

Open the URL, authorize, then copy the returned `code` from your callback URL.

### 3.2 Exchange code

```bash
repo/.cache/bin/token-manager-agent exchange "<AUTHORIZATION_CODE>"
```

Validate:

```bash
repo/.cache/bin/token-manager-agent token-info
```

## 4. Deploy

### Option A: Makefile path (recommended)

```bash
cd /srv/42_Network/repo
make deploy CAMPUS_ID=76
```

This runs:
1. Environment checks
2. Docker startup
3. DB init
4. Metadata fetch
5. Orchestra worker
6. Cron setup for user polling

### Option B: Orchestrator script

```bash
cd /srv/42_Network
CAMPUS_ID=76 /srv/42_Network/repo/.cache/bin/toolkit-agent deploy_orchestra
```

Useful flags:

```bash
CAMPUS_ID=76 /srv/42_Network/repo/.cache/bin/toolkit-agent deploy_orchestra --dry-run
CAMPUS_ID=76 /srv/42_Network/repo/.cache/bin/toolkit-agent deploy_orchestra --skip-monitor
CAMPUS_ID=76 /srv/42_Network/repo/.cache/bin/toolkit-agent deploy_orchestra --skip-workers
```

## 5. Verify

```bash
cd /srv/42_Network/repo
make status
./.cache/bin/monitoring-agent system_health
./.cache/bin/toolkit-agent pipeline_manager status
```

Key outputs:
- Docker containers `transcendence_*` should be running.
- Queue files under `/srv/42_Network/runtime/backlog/` should exist.
- Logs should be present under `/srv/42_Network/runtime/logs/`.

## 6. Endpoints

Open in browser:

```text
http://localhost:<WEB_PORT>/
http://localhost:<WEB_PORT>/api/
http://localhost:<WEB_PORT>/godot/
http://localhost:<WEB_PORT>/health
```

`<WEB_PORT>` comes from `.env` (`WEB_PORT`, default in compose fallback is 8000).

## 7. Core Operations

Refresh token:

```bash
/srv/42_Network/repo/.cache/bin/token-manager-agent refresh
```

Run stable metadata update:

```bash
/srv/42_Network/repo/.cache/bin/toolkit-agent update_all_cursus_21_core
```

Force metadata refresh:

```bash
/srv/42_Network/repo/.cache/bin/toolkit-agent update_all_cursus_21_core --force
```

Pipeline process control:

```bash
/srv/42_Network/repo/.cache/bin/toolkit-agent pipeline_manager start
/srv/42_Network/repo/.cache/bin/toolkit-agent pipeline_manager status
/srv/42_Network/repo/.cache/bin/toolkit-agent pipeline_manager stop
```

## 8. Troubleshooting

### `REFRESH_TOKEN not in .env`

Set `REFRESH_TOKEN` in `/srv/42_Network/.env` and rerun deploy.

### `.oauth_state` missing

Run the first OAuth exchange step:

```bash
/srv/42_Network/repo/.cache/bin/token-manager-agent exchange "<AUTHORIZATION_CODE>"
```

### DB connectivity failures

```bash
cd /srv/42_Network/repo
docker compose -f infra/docker-compose.yml ps
docker compose -f infra/docker-compose.yml logs db
```

### Token expired during runtime

```bash
/srv/42_Network/repo/.cache/bin/token-manager-agent refresh
```

### Large or noisy logs

```bash
/srv/42_Network/repo/.cache/bin/cleanup-logs-agent
```

Aggressive cleanup of old `orchestra_*.log`:

```bash
/srv/42_Network/repo/.cache/bin/cleanup-logs-agent --aggressive
```

## 9. Stop and Cleanup

```bash
cd /srv/42_Network/repo
make down
make clean
```

Destructive cleanup:

```bash
make fclean
```

`make fclean` removes containers/volumes/images and archives current logs/exports.
