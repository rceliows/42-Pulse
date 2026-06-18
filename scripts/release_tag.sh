#!/usr/bin/env bash
# Tag the currently-built transcendence/* images with the current git short SHA
# (in addition to :latest) and record it in runtime/releases.log so a later
# `make rollback TAG=<sha>` can restore them.
set -euo pipefail

require_var() {
  local name="$1"
  if [[ -z "${!name:-}" ]]; then
    echo "ERROR: missing required variable: $name" >&2
    exit 1
  fi
}

require_var "ROOT_DIR"
require_var "RUNTIME_DIR"

IMAGES=(api detector fetcher upserter maintenance-scheduler)

sha="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || true)"
if [[ -z "$sha" ]]; then
  echo "❌ Could not determine git SHA (not a git repo?)" >&2
  exit 1
fi

mkdir -p "$RUNTIME_DIR/logs/state"
releases_log="$RUNTIME_DIR/logs/state/releases.log"

for name in "${IMAGES[@]}"; do
  src="transcendence/${name}:latest"
  dest="transcendence/${name}:${sha}"
  if docker image inspect "$src" >/dev/null 2>&1; then
    docker tag "$src" "$dest"
    echo "✅ Tagged ${dest}"
  else
    echo "⚠️  Skipping ${name}: ${src} not found locally" >&2
  fi
done

echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) ${sha}" >> "$releases_log"
echo "📝 Recorded release ${sha} in ${releases_log}"
