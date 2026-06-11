# Tools

`tools/` contains non-containerized C++ command-line agents.

These binaries are built by the root `Makefile` into `runtime/cache/bin/`.

## Modules

- `api42-client/` -> `api42-client-agent`
- `token-manager/` -> `token-manager-agent`
- `ops/` -> `ops-agent`
- `orchestra/` -> `orchestration-agent`

## Scope

- `app/` is reserved for containerized services used by `infra/docker-compose.yml`.
- `tools/` is reserved for CLI/batch agents invoked by `make` targets and scripts.
