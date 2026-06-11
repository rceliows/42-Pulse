# Phase 1 (Historical Snapshot)

This document summarizes the Phase 1 milestone completed in December 2025.

## Snapshot Metadata

- Completion date: December 12, 2025
- Focus: stable metadata tables
- Result: reproducible metadata ingest pipeline with automated loading

## What Phase 1 Delivered

- Stable reference ingestion for cursus/campuses/projects/coalitions/achievements
- Derived relation extraction for campus-project and session mappings
- Repeatable load process into PostgreSQL
- Basic operational logs and maintenance scripts

## Current Equivalent Commands (as of Feb 2026)

Run stable metadata refresh:

```bash
cd /srv/42_Network/repo
./.cache/bin/toolkit-agent update_all_cursus_21_core
```

Force full re-fetch:

```bash
cd /srv/42_Network/repo
./.cache/bin/toolkit-agent update_all_cursus_21_core --force
```

Token refresh (if needed):

```bash
cd /srv/42_Network/repo
./.cache/bin/token-manager-agent refresh
```

## Historical Data Notes

Original Phase 1 reports include row counts and timing snapshots from December 2025.
Treat those numbers as historical baselines, not guaranteed current values.

## Validation Checklist for Re-run

```bash
cd /srv/42_Network/repo

# services
make status

# latest stable pipeline log
tail -n 80 logs/update_cursus_21_core.log

# basic DB check
docker compose -f infra/docker-compose.yml exec -T db psql -U "${DB_USER:-api42}" -d "${DB_NAME:-api42}" -c "SELECT 1;"
```

## What Changed Since Phase 1

- Worker and orchestration scripts expanded for ongoing user sync.
- Monitoring tooling moved to `monitoring-agent` entrypoints (`app/monitoring/src`).
- Some older pre-2026 script paths were removed during the C++/toolkit migration.

## Related Docs

- `docs/PHASE2_ANALYSIS.md`
- `docs/PIPELINE_FLOW.md`
- `repo/docs/12_COMMAND_REFERENCE.md`
