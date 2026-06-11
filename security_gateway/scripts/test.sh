#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

log "Checking shell scripts..."
for script in "$SECURITY_GATEWAY_DIR"/scripts/*.sh; do
  bash -n "$script"
done

if command -v node >/dev/null 2>&1; then
  log "Checking OAuth gateway JavaScript syntax..."
  node --check "$SECURITY_GATEWAY_DIR/oauth-gateway/src/server.js" >/dev/null
elif command -v docker >/dev/null 2>&1; then
  log "Checking OAuth gateway JavaScript syntax through Docker..."
  docker run --rm \
    -v "$SECURITY_GATEWAY_DIR/oauth-gateway:/app:ro" \
    -w /app \
    node:22-alpine \
    node --check src/server.js >/dev/null
else
  log "Skipping Node syntax check because node is not installed on the host."
fi

log "Checking Docker Compose syntax..."
export_context
STACK_NETWORK_NAME="${STACK_NETWORK_NAME:-dummy_transcendence}" docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" config >/dev/null

if compose ps --services --filter status=running 2>/dev/null | grep -q '^waf$'; then
  log "Running live WAF smoke checks..."
  curl -ksS "https://127.0.0.1:${WAF_PORT}/healthz" >/dev/null
  sensitive_status="$(curl -ksS -o /dev/null -w '%{http_code}' "https://127.0.0.1:${WAF_PORT}/.env")"
  if [[ "$sensitive_status" != "403" ]]; then
    fail "Expected WAF to block /.env with 403, got $sensitive_status."
  fi
else
  log "Skipping live WAF smoke checks because the security_gateway WAF is not running."
fi

log "security_gateway checks passed."
