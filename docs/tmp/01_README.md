# 42 Network Runtime Repository

This `repo/` directory contains the runnable stack for 42 Network:
- Docker services (DB, web, API, agents)
- Data ingestion and sync scripts
- Queue-based worker pipeline
- Monitoring and operations tooling

## Stack

- PostgreSQL 16 (`transcendence_db`)
- Nginx static/API serving (`transcendence_web`)
- Node API (`transcendence_api`)
- Agents (`detector`, `fetcher`, `upserter`, `backlog-worker`, `ops`)

## Main Data Flows

### Stable metadata refresh

```bash
./.cache/bin/toolkit-agent update_all_cursus_21_core
```

### Campus user sync

```bash
CAMPUS_ID=76 ./.cache/bin/toolkit-agent fetch_users
```

### Continuous worker pipeline

```bash
./.cache/bin/toolkit-agent pipeline_manager start
```

## Directory Overview

```text
repo/
├── app/
│   ├── api/
│   ├── detector/
│   ├── fetcher/
│   ├── fetcher-external/
│   ├── godot/
│   ├── upserter/
│   ├── backlog-worker/
│   ├── ops/
│   └── nginx/
├── infra/
│   └── docker-compose.yml
├── sql/
│   └── schema.sql
├── Makefile
├── runtime data (outside repo/)
│   ├── ../runtime/logs/
│   ├── ../runtime/exports/
│   ├── ../runtime/backlog/
│   └── ../runtime/cache/
├── scripts/
│   ├── agents/
│   ├── monitoring/
│   ├── pipeline/
│   ├── update_stable_tables/
│   ├── helpers/ (kept mostly empty; use toolkit-agent commands)
│   └── token-manager-agent (compiled; invoked via `.cache/bin`)
├── app/toolkit/
│   ├── src/ (toolkit-agent + orchestration-agent)
│   └── config/
└── docs/
```

## First Run

See `02_QUICK_START.md`.

## Operational Reference

Use `12_COMMAND_REFERENCE.md` for current commands.

## Historical Notes

Some design docs in `docs/` were written during earlier phases and keep that context intentionally.
For live command execution, always prefer the command reference file.
