# App Services

`app/` contains the containerized runtime services used by `infra/docker-compose.yml`.

## Service overview

- `api/`
  - Node.js backend API used by admin/pages.
  - Exposes REST endpoints and WebSocket streams.
  - Reads runtime state from `/app/logs`, `/app/backlog`, `/app/exports`.

- `web/`
  - Nginx frontend container.
  - Serves static pages (`message.html`, `events.html`, `events_dashboard.html`, `admin.html`).
  - Proxies API calls to the `api` service.

- `detector/`
  - Polling worker that detects user/event changes from 42 API.
  - Produces queue inputs in `runtime/backlog` (live pipeline entry point).
  - Binary entrypoint: `/usr/local/bin/detector-agent --loop`.

- `fetcher/`
  - Fetch worker that consumes user IDs from queue and fetches full user payloads.
  - Writes user payload exports to `runtime/exports/09_users`.
  - Binary entrypoint: `/usr/local/bin/fetcher-agent`.
  - Used by multiple compose services:
    - `fetcher_internal`
    - `fetcher-external-1`
    - `fetcher-external-2`
    - `fetcher-external-3`

- `upserter/`
  - DB writer worker.
  - Consumes queue/export inputs and upserts into PostgreSQL.
  - Binary entrypoint: `/usr/local/bin/upserter-agent`.
  - Used in two modes:
    - `UPSERTER_MODE=users` (`upserter-users`)
    - `UPSERTER_MODE=events` (`upserter-events`)

- `maintenance-scheduler/`
  - Periodic operations runner.
  - Executes maintenance cycle (token refresh, backup, health snapshot, cleanup logs) on interval.
  - Binary entrypoint: `/usr/local/bin/maintenance-scheduler-agent`.

## Build/runtime notes

- Worker images compile their C++ agents during Docker build.
- Runtime writable data lives in `/srv/42_Network/runtime` (bind-mounted into containers).
- Operational CLI tools are in `tools/` (outside `app/`).
