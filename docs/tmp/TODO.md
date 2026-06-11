# 42_Network TODO (Organization and Cleanup)

Last reviewed: February 13, 2026

## Priority 0 - Organization Hygiene

- [ ] Decide canonical docs scope between root `docs/` and `repo/docs/`.
- [ ] Move stale/deprecated operational docs to archive buckets.
- [ ] Keep one operational command source of truth (`repo/docs/12_COMMAND_REFERENCE.md`).
- [ ] Add explicit "historical" tags to phase documents that are no longer runbooks.

## Priority 1 - Script Path Consistency

- [ ] Remove remaining references to removed script paths from older docs.
- [ ] Ensure every script invoked by orchestration exists under non-legacy paths.
- [ ] Add compatibility wrappers only where needed, with clear comments.
- [ ] Audit root-level compatibility wrappers in `/srv/42_Network/scripts` quarterly.

## Priority 2 - Runtime Reliability

- [ ] Align `.env` validation with actual token bootstrap flow (`.oauth_state` creation).
- [ ] Add preflight check for missing `repo/.oauth_state` in deployment path.
- [ ] Add smoke command that validates token + API + DB in one step.
- [ ] Introduce structured error codes in major scripts for easier automation.

## Priority 3 - Documentation Quality

- [ ] Standardize dates and "last updated" fields across active docs.
- [ ] Replace historical row-count claims with "snapshot at date" wording.
- [ ] Add one "current architecture" diagram and link it from all entry docs.
- [ ] Add troubleshooting matrix (symptom -> command -> expected output).

## Priority 4 - Backups and Recovery

- [ ] Implement an automated DB backup executable (`backup-database-agent`) or remove backup mention from ops loop.
- [ ] Define retention policy and automated prune for backup artifacts.
- [ ] Add restore drill checklist and monthly validation cadence.

## Priority 5 - Repo Structure Cleanup

- [ ] Review empty/top-level directories (`infra/`, `tests/`) and keep/remove intentionally.
- [ ] Ensure generated artifacts are ignored consistently.
- [ ] Separate runtime outputs (`logs/`, `exports/`, caches) from versioned assets.
- [ ] Add a concise `CONTRIBUTING.md` for folder conventions.

## Suggested Execution Order

1. Normalize script references and wrappers.
2. Consolidate docs entry points and command reference.
3. Add deployment/token preflight improvements.
4. Implement backup script or remove backup expectation.
5. Archive historical docs and reduce navigation noise.
