# Continuation Context (Current Snapshot)

Purpose: compact continuity memo for future sessions working in `repo/`.

## Snapshot Date

- Reviewed: February 13, 2026
- Historical phase docs: mostly authored in December 2025

## Current Operational Truth

- Canonical runtime entrypoints are compiled agents (`/usr/local/bin/*-agent`) and `toolkit-agent`.
- Stable metadata updater:
  - `toolkit-agent update_all_cursus_21_core`
- Orchestration:
  - `toolkit-agent deploy_orchestra`
  - `toolkit-agent orchestra`
- Worker control:
  - `toolkit-agent pipeline_manager`
- Monitoring:
  - `monitoring-agent system_health`
  - `monitoring-agent events_diff`
  - `monitoring-agent live_delta_monitor`

## Known Structural Constraints

- Repository is in active migration; some docs reference removed paths.
- `scripts/legacy/*.sh` was removed; treat any remaining references as historical only.
- `orchestration-agent deploy_orchestra` invokes monitor checks through `monitoring-agent` (`live_delta_monitor` + `pipeline_monitor`).

## Data and Queues

Main runtime dirs:
- `exports/`
- `logs/`
- `.backlog/`

Primary queues:
- `.backlog/fetch_queue_internal.txt`
- `.backlog/fetch_queue_external.txt`
- `.backlog/process_queue.txt`
- `.backlog/events_queue.jsonl`

## High-Value Validation Commands

```bash
# Services
make -C /srv/42_Network/repo status

# Health snapshot
/srv/42_Network/repo/.cache/bin/monitoring-agent system_health

# Pipeline status
/srv/42_Network/repo/.cache/bin/toolkit-agent pipeline_manager status

# Token status
/srv/42_Network/repo/.cache/bin/token-manager-agent token-info
```

## Documentation Rule

For commands, trust `12_COMMAND_REFERENCE.md` first.
Use deeper phase docs for context/history, not as the first operational source.
