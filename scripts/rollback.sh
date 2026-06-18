#!/usr/bin/env bash
# Roll back the custom transcendence/* services to a previously-released image
# tag (a git short SHA recorded by scripts/release_tag.sh in
# runtime/logs/state/releases.log), then restart those services without
# rebuilding.
#
# Usage: TAG=<sha> ./scripts/rollback.sh
set -euo pipefail

require_var() {
  local name="$1"
  if [[ -z "${!name:-}" ]]; then
    echo "ERROR: missing required variable: $name" >&2
    exit 1
  fi
}

require_var "TAG"
require_var "ROOT_DIR"
require_var "RUNTIME_DIR"

IMAGES=(api detector fetcher upserter maintenance-scheduler)
SERVICES=(api detector fetcher_internal fetcher-external-1 fetcher-external-2 fetcher-external-3 upserter-users upserter-events maintenance-scheduler)

missing=0
for name in "${IMAGES[@]}"; do
  image="transcendence/${name}:${TAG}"
  if ! docker image inspect "$image" >/dev/null 2>&1; then
    echo "❌ Missing image ${image} - cannot roll back to TAG=${TAG}" >&2
    missing=1
  fi
done

if [[ "$missing" -ne 0 ]]; then
  releases_log="$RUNTIME_DIR/logs/state/releases.log"
  echo "Available releases:" >&2
  [[ -f "$releases_log" ]] && tail -10 "$releases_log" >&2 || echo "  (no releases recorded yet)" >&2
  exit 1
fi

echo "⏪ Rolling back to TAG=${TAG} (no rebuild)..."
TAG="$TAG" docker compose -f "$ROOT_DIR/infra/docker-compose.yml" up -d --no-build "${SERVICES[@]}"

mkdir -p "$RUNTIME_DIR/logs/state"
echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) rollback ${TAG}" >> "$RUNTIME_DIR/logs/state/releases.log"
echo "✅ Rolled back to ${TAG}"
