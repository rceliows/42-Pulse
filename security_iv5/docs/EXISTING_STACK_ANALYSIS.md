# Existing Stack Analysis

This analysis was produced without modifying the existing project files.

## Existing Architecture

The current project already contains:

- PostgreSQL in [`infra/docker-compose.yml`](/home/tazokami/Projets/Campus19/42_Network/infra/docker-compose.yml)
- an Nginx `web` service in [`infra/docker-compose.yml`](/home/tazokami/Projets/Campus19/42_Network/infra/docker-compose.yml)
- a Node.js API with WebSocket support in [`app/api/server.js`](/home/tazokami/Projets/Campus19/42_Network/app/api/server.js)
- detector, fetcher, upserter and maintenance workers in the same compose stack
- deployment orchestration in [`Makefile`](/home/tazokami/Projets/Campus19/42_Network/Makefile)

## Current Security-Relevant Findings

1. Secrets are still file-based.
   Current values are loaded from sibling files outside the repo:
   - `../.env`
   - `../transcendance.config`
   - `repo/.oauth_state`

2. The current public entrypoint is the existing `web` Nginx service.
   It already serves static pages and proxies `/api/` and WebSocket traffic to `api`.

3. The current compose network is a shared internal bridge network.
   The WAF can therefore be added without changing the existing services by joining the same Docker network and proxying to the existing `web` hostname.

4. The root `Makefile` is under active development.
   The worktree currently contains a merge conflict on [`Makefile`](/home/tazokami/Projets/Campus19/42_Network/Makefile), so editing the current deployment path would be high-risk.

## Non-Destructive Integration Strategy

To respect the no-touch constraint, IV.5 is implemented as a sidecar security module:

1. Vault runs in its own isolated compose project and network.
2. Vault is TLS-enabled and uses integrated storage.
3. Existing secrets are imported into Vault from the current files.
4. A local AppRole is created for runtime-grade secret reads.
5. A rendered env file is generated from Vault for future secure restarts.
6. A separate WAF compose project joins the current stack network and proxies to the existing `web` service.

## Why This Shape Fits The Current Repo

- No root file is changed.
- No existing service definition is overwritten.
- The current app remains deployable by the team exactly as-is.
- The cybersecurity module can be demonstrated independently during evaluation.
