#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

ensure_stack_network_or_die

if ! grep -q '^SESSION_SECRET=' "$RENDERED_OAUTH_ENV_FILE" 2>/dev/null; then
  fail "Missing rendered OAuth env. Run 'make bootstrap' or 'make render' first."
fi

log "Starting security_gateway OAuth gateway and WAF on network $STACK_NETWORK_NAME..."
compose up -d --build oauth-gateway waf

log "Protected entrypoint: https://localhost:${WAF_PORT}"
