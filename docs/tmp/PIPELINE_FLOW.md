# 42 Network Pipeline Flow

This document reflects the current worker layout under `app/` and runtime orchestration via `toolkit-agent`.

If you are new to this codebase, read `ARCHITECTURE_FIRST_READ.md` first, then return here.

## Runtime Architecture

```text
42 API
  -> detector agent (/usr/local/bin/detector-agent --loop)
     -> runtime/backlog/fetch_queue_internal.txt
     -> runtime/backlog/fetch_queue_external.txt
     -> runtime/backlog/events_queue.jsonl
  -> fetcher agents (/usr/local/bin/fetcher-agent internal + external queues)
     -> runtime/exports/09_users/campus_<id>/user_<id>.json
     -> runtime/backlog/process_queue.txt
  -> upserter agent (/usr/local/bin/upserter-agent)
     -> PostgreSQL users and related tables
  -> backlog worker agent (/usr/local/bin/backlog-worker-agent)
     -> enrichment and follow-up relation sync
```

## Stable Metadata Pipeline

The stable metadata refresh path is:

```bash
./.cache/bin/toolkit-agent update_all_cursus_21_core
```

Internally:
1. Fetch metadata (`toolkit-agent fetch_cursus_21_core_metadata`)
2. Validate exported JSON bundles
3. Load tables with `update_stable_tables/*`
4. Write metrics/logs to `runtime/logs/update_cursus_21_core.log`

## Orchestra Deployment Flow

```bash
CAMPUS_ID=76 ./.cache/bin/toolkit-agent deploy_orchestra
```

Orchestrator sequence:
1. `docker compose -f infra/docker-compose.yml up -d`
2. `toolkit-agent check_environment`
3. `toolkit-agent init_db`
4. Monitoring helpers (`monitoring-agent live_delta_monitor`, `monitoring-agent pipeline_monitor` when enabled)
5. Worker run via `toolkit-agent orchestra`

## Process Control

Use the pipeline manager:

```bash
./.cache/bin/toolkit-agent pipeline_manager start
./.cache/bin/toolkit-agent pipeline_manager status
./.cache/bin/toolkit-agent pipeline_manager stop
```

Status reports:
- detector/fetcher/upserter PID status
- queue sizes from `runtime/backlog/`
- quick health visibility for stage bottlenecks

## Queue Files

Main queue files:

- `runtime/backlog/fetch_queue_internal.txt`
- `runtime/backlog/fetch_queue_external.txt`
- `runtime/backlog/process_queue.txt`
- `runtime/backlog/events_queue.jsonl`

Interpretation:
- Growing fetch queues: API-side pressure or detector spike
- Growing process queue: DB/upserter pressure
- Growing events queue without downstream handling: event consumer lag

## Monitoring Commands

```bash
repo/.cache/bin/monitoring-agent system_health
repo/.cache/bin/monitoring-agent events_diff 50
repo/.cache/bin/monitoring-agent live_delta_monitor
```

These scripts provide:
- container/process state
- API reachability checks
- queue depth snapshots
- latest detector change summaries

## Logging

Log directory:

```text
runtime/logs/
```

Typical files:
- `detect_changes.log`
- `fetcher.log`
- `upserter.log`
- `ops_agent.log`
- `update_cursus_21_core.log`
- `orchestra_*.log`

Cleanup helper:

```bash
repo/.cache/bin/cleanup-logs-agent
```
