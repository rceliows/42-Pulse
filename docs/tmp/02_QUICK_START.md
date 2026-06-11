# Quick Start

This is the shortest reliable path to get the runtime stack up.

## 1. Configure Environment

From `/srv/42_Network`:

```bash
cp repo/.env.example .env
```

Set required values in `.env`:
- `CLIENT_ID`
- `CLIENT_SECRET`
- `REDIRECT_URI`
- `SCOPE`
- `REFRESH_TOKEN`

## 2. Bootstrap OAuth State

If `repo/.oauth_state` does not exist, run an auth-code exchange:

```bash
repo/.cache/bin/token-manager-agent exchange "<AUTHORIZATION_CODE>"
```

Validate:

```bash
repo/.cache/bin/token-manager-agent token-info
```

## 3. Deploy

```bash
cd /srv/42_Network/repo
make deploy CAMPUS_ID=76
```

## 4. Verify

```bash
make status
./.cache/bin/monitoring-agent system_health
./.cache/bin/toolkit-agent pipeline_manager status
```

## 5. First Stable Metadata Refresh (Optional)

```bash
./.cache/bin/toolkit-agent update_all_cursus_21_core --force
```

## 6. Logs and Queues

```bash
ls -lah ../runtime/logs | tail
wc -l ../runtime/backlog/fetch_queue_internal.txt ../runtime/backlog/fetch_queue_external.txt ../runtime/backlog/process_queue.txt
```

## Common Recovery Commands

```bash
# Refresh token
./.cache/bin/token-manager-agent refresh

# Restart pipeline workers
./.cache/bin/toolkit-agent pipeline_manager restart

# Cleanup logs
./.cache/bin/cleanup-logs-agent
```
