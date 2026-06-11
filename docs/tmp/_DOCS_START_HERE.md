# Documentation Entry Point

Use this file as the start point for the `repo/` documentation set.

## Read In This Order

1. `docs/00_HIERARCHY_START_HERE.md` (map of all docs)
2. `docs/01_README.md` (what this repo does)
3. `docs/02_QUICK_START.md` (first deployment)
4. `docs/12_COMMAND_REFERENCE.md` (daily operations)

## Fast Paths

- Deploy now: `docs/02_QUICK_START.md`
- Runbook/commands: `docs/12_COMMAND_REFERENCE.md`
- Architecture overview: `docs/20_CURSUS_21_DATA_PIPELINE.md`
- Architecture first-read (onboarding): `docs/ARCHITECTURE_FIRST_READ.md`
- Monitoring: `docs/32_MONITORING_COMPLETE.md`

## Historical vs Current

Some deep-dive docs were written during December 2025 refactors and still include legacy paths.

Current canonical runtime entrypoints are:
- `toolkit-agent` orchestration/update/helper commands
- `token-manager-agent`
- compiled worker agents in `app/*/src` (detector/fetcher/upserter/backlog-worker/ops)
- monitoring utilities in `app/monitoring/src` (runtime via `monitoring-agent`)

When command examples conflict, prefer `docs/12_COMMAND_REFERENCE.md`.
