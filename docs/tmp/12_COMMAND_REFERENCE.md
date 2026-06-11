# Command Reference

This file lists commands that match the current repository layout.

## Deployment

```bash
cd /srv/42_Network/repo
make check
make deploy CAMPUS_ID=76
make status
make logs
```

## Tokens

```bash
# One-time auth-code exchange
./.cache/bin/token-manager-agent exchange "<AUTHORIZATION_CODE>"

# Refresh access token
./.cache/bin/token-manager-agent refresh

# Inspect token metadata
./.cache/bin/token-manager-agent token-info

# Call API quickly
./.cache/bin/token-manager-agent call /v2/me | jq
```

## Stable Metadata Pipeline

```bash
# Standard run (cache aware)
./.cache/bin/toolkit-agent update_all_cursus_21_core

# Force fresh pull
./.cache/bin/toolkit-agent update_all_cursus_21_core --force
```

## Orchestration

```bash
# Full orchestrated flow
CAMPUS_ID=76 ./.cache/bin/toolkit-agent deploy_orchestra

# Useful flags
CAMPUS_ID=76 ./.cache/bin/toolkit-agent deploy_orchestra --dry-run
CAMPUS_ID=76 ./.cache/bin/toolkit-agent deploy_orchestra --skip-monitor
CAMPUS_ID=76 ./.cache/bin/toolkit-agent deploy_orchestra --skip-workers

# Worker run only
CAMPUS_ID=76 ./.cache/bin/toolkit-agent orchestra
```

## Pipeline Process Control

```bash
./.cache/bin/toolkit-agent pipeline_manager start
./.cache/bin/toolkit-agent pipeline_manager status
./.cache/bin/toolkit-agent pipeline_manager restart
./.cache/bin/toolkit-agent pipeline_manager stop
```

## Monitoring

```bash
./.cache/bin/monitoring-agent system_health
./.cache/bin/monitoring-agent events_diff 50
./.cache/bin/monitoring-agent live_delta_monitor
./.cache/bin/monitoring-agent pipeline_monitor
```

## Database Access

```bash
docker compose -f infra/docker-compose.yml exec -T db psql -U "${DB_USER:-api42}" -d "${DB_NAME:-api42}" -c "SELECT 1;"
```

Quick integrity snapshot:

```bash
docker compose -f infra/docker-compose.yml exec -T db psql -U "${DB_USER:-api42}" -d "${DB_NAME:-api42}" -c "SELECT 'users' AS table_name, COUNT(*) FROM users UNION ALL SELECT 'projects', COUNT(*) FROM projects;"
```

## Users Fetch and Sync

```bash
# Single campus
CAMPUS_ID=76 ./.cache/bin/toolkit-agent fetch_users

# All campuses mode (heavy)
CAMPUS_ID=ALL ./.cache/bin/toolkit-agent fetch_users

# Dry run
CAMPUS_ID=76 ./.cache/bin/toolkit-agent fetch_users --dry-run
```

## Cleanup

```bash
./.cache/bin/cleanup-logs-agent
./.cache/bin/cleanup-logs-agent --aggressive
```

## Docker Lifecycle

```bash
make down
make clean
make fclean
```

`make fclean` is destructive (drops volumes and images).
