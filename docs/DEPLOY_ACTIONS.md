# Deploy Actions (`make deploy`)

This document describes the current deploy flow, step by step.

## Official flows

1. Standard deploy

```bash
make deploy
```

2. OAuth bootstrap + deploy

```bash
make deploy CODE="<AUTHORIZATION_CODE>"
```

## Runtime tree created by `prepare_runtime.sh`

The script creates missing folders only, under the parent of `ROOT_DIR`.
Existing folders/files are kept.

```text
<ROOT_DIR parent>/
  backups/
  data/
    postgres/
  runtime/
    backlog/
    cache/
      bin/
      raw_detect/
    cleanup/
    exports/
    logs/
      agents/
      archive/
        health/
      ops/
      pids/
      state/
```

## Deploy sequence (current)

1. Optional OAuth exchange
   - If `CODE` is provided: `make exchange CODE=...`.
   - Updates `repo/.oauth_state`.
   - If `CODE` is not provided: step is skipped.

2. Validate deploy config
   - Runs `scripts/validate_config.sh`.
   - Requires `../transcendance.config`.
   - `ROOT_DIR`:
     - if set in config: must resolve to a valid repo root (`infra/docker-compose.yml` + `app/`);
     - if missing: defaults to where Make was launched (`REPO_ROOT`).
   - Minimum config keys validated for deploy:
     - `CAMPUS_ID` (required, numeric),
     - `ROOT_DIR` (optional, default from Make launch path as above).
   - Minimum DB keys validated (used by step 4):
     - `DB_PORT`, `DB_NAME`, `DB_USER`, `DB_PASSWORD`, `DB_DATA_DIR`.

3. Prepare runtime folders
   - Runs `scripts/prepare_runtime.sh`.
   - Uses `ROOT_DIR` and creates folders in `ROOT_DIR` parent:
     - `runtime/` (full runtime tree),
     - `data/postgres/`,
     - `backups/`.
   - Non-destructive: existing data is preserved (no cleanup, no wipe).

4. Start DB + create/check database
   - Runs `make up-db`.
   - Reconciles `DB_DATA_DIR` ownership to PostgreSQL container UID/GID `999:999`.
   - Uses the actual `/var/run/docker.sock` group GID when a stale `DOCKER_GID` is configured.
   - Starts `db` container.
   - Waits for readiness (`pg_isready`).
   - Creates DB if missing.
   - Verifies DB connection with `SELECT 1`.
   - Applies `sql/schema.sql` and verifies every table declared in that file exists before app services start.

5. Build local agents
   - Runs `make tooling`.
   - Builds local binaries in `runtime/cache/bin`:
     - `orchestration-agent`
     - `ops-agent`
     - `token-manager-agent`
     - `api42-client-agent`
   - This happens before service startup so containers that rely on these binaries can start cleanly.
   - `detector` and `fetcher` call `api42-client-agent`, which performs `ensure-fresh` token flow through `token-manager-agent`.

6. Build + start core services (detector included, maintenance deferred)
   - Runs `make up-services-core`.
   - Executes `docker compose up -d --build` for app services except `db` and `maintenance-scheduler`.
   - Includes: `api`, `web`, `detector`, fetchers, and upserters.

7. Run orchestration flow
   - Runs `orchestration-agent orchestra`.

8. Start maintenance scheduler (post-orchestra)
   - Runs `make maintenance-auto-start`.
   - Scheduler first cycle starts immediately and includes backup + health snapshot.

9. Write fresh system health snapshot
   - Runs `ops-agent system_health`.

## Notes

- `make deploy` does not call `validate_env.sh`.
- `make deploy` does not use the `make check` wrapper.
- `make exchange` does not run runtime preparation.
- `make fclean` is protected and does not remove `data/`.
