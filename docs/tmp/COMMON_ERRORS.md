# Common Errors and Fixes

## 1. Running scripts from wrong directory

Error:

```text
bash: token-manager-agent: No such file or directory
```

Cause:
- You are in `/srv/42_Network` but runnable binaries are in `/srv/42_Network/repo/.cache/bin`.

Fix:

```bash
# Option A: run inside repo
cd /srv/42_Network/repo
./.cache/bin/token-manager-agent token-info

# Option B: use absolute path
/srv/42_Network/repo/.cache/bin/token-manager-agent token-info
```

## 2. `.env` exists but deploy check still fails

Error:

```text
REFRESH_TOKEN not in .env
```

Cause:
- `toolkit-agent check_environment` checks `/srv/42_Network/.env` explicitly.

Fix:
- Ensure `/srv/42_Network/.env` exists and contains `REFRESH_TOKEN=...`.

## 3. Token helper fails with "No refresh token saved"

Error:

```text
No refresh token saved. Run exchange first.
```

Cause:
- `repo/.oauth_state` was never bootstrapped.

Fix:

```bash
/srv/42_Network/repo/.cache/bin/token-manager-agent exchange "<AUTHORIZATION_CODE>"
```

Then retry refresh/call commands.

## 4. `docker compose` starts but app still unavailable

Checks:

```bash
cd /srv/42_Network/repo
docker compose -f infra/docker-compose.yml ps
docker compose -f infra/docker-compose.yml logs web --tail=80
docker compose -f infra/docker-compose.yml logs api --tail=80
```

Look for container health and port binding errors.

## 5. Pipeline appears idle

Check queue and process status:

```bash
/srv/42_Network/repo/.cache/bin/toolkit-agent pipeline_manager status
wc -l /srv/42_Network/runtime/backlog/fetch_queue_internal.txt \
      /srv/42_Network/runtime/backlog/fetch_queue_external.txt \
      /srv/42_Network/runtime/backlog/process_queue.txt
```

If all queues stay at 0, verify detector is running and API token is valid.

## 6. Monitoring command path mismatch

If docs or scripts call `monitoring-agent pipeline_monitor`, it now exists as a compatibility entrypoint.
Use any of these:

```bash
/srv/42_Network/repo/.cache/bin/monitoring-agent pipeline_monitor
/srv/42_Network/repo/.cache/bin/monitoring-agent system_health
```

## 7. Log files grow too large

Use cleanup helper:

```bash
/srv/42_Network/repo/.cache/bin/cleanup-logs-agent
```

Aggressive pruning:

```bash
/srv/42_Network/repo/.cache/bin/cleanup-logs-agent --aggressive
```
