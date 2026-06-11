# Phase 2 Sprint 1 Incremental Test Plan (Refreshed)

This plan keeps the original phased idea but uses current script paths.

## Goal

Validate incremental users synchronization safely before scaling up load.

## Stage 1: API and token sanity (small sample)

Commands:

```bash
cd /srv/42_Network/repo
./.cache/bin/token-manager-agent token-info
./.cache/bin/token-manager-agent call-export "/v2/cursus/21/cursus_users?filter[kind]=student&per_page=10" /tmp/stage1_users.json
jq 'length' /tmp/stage1_users.json
```

Expected:
- token helper works
- non-empty JSON array is returned

## Stage 2: Campus fetch dry-run

Commands:

```bash
cd /srv/42_Network/repo
CAMPUS_ID=76 ./.cache/bin/toolkit-agent fetch_users --dry-run
```

Expected:
- script runs without crashing
- logs written under `logs/fetch_users_*`

## Stage 3: Single-campus real fetch

Commands:

```bash
cd /srv/42_Network/repo
CAMPUS_ID=76 ./.cache/bin/toolkit-agent fetch_users
ls -lah exports/09_users/campus_76/
jq 'length' exports/09_users/campus_76/all.json
```

Expected:
- `all.json` exists
- positive row count

## Stage 4: Pipeline integration

Commands:

```bash
cd /srv/42_Network/repo
./.cache/bin/toolkit-agent pipeline_manager start
sleep 30
./.cache/bin/toolkit-agent pipeline_manager status
```

Expected:
- detector/fetcher/upserter report running
- queues appear and update over time

## Stage 5: Monitoring and diff checks

Commands:

```bash
cd /srv/42_Network/repo
./.cache/bin/monitoring-agent system_health
./.cache/bin/monitoring-agent events_diff 20
./.cache/bin/monitoring-agent live_delta_monitor
```

Expected:
- health output without hard failures
- events can be inspected from queue log

## Stage 6: Rollback-ready validation

Before scaling to larger campuses:
- confirm `cleanup-logs-agent` works
- confirm token refresh works
- confirm DB accepts writes (`SELECT 1` + sample row checks)

Commands:

```bash
cd /srv/42_Network/repo
./.cache/bin/token-manager-agent refresh
docker compose -f infra/docker-compose.yml exec -T db psql -U "${DB_USER:-api42}" -d "${DB_NAME:-api42}" -c "SELECT 1;"
```

## Exit Criteria

Move to larger campuses only when:
- no repeated token failures
- no persistent queue growth without drain
- no DB write errors in upserter logs
- monitoring scripts produce coherent output
