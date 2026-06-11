#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

require_file "$SECURITY_DIR/waf/certs/ca.crt"
require_file "$SECURITY_DIR/waf/certs/waf.crt"
require_file "$SECURITY_DIR/waf/certs/waf.key"

ensure_stack_network_or_die

log "Starting WAF on stack network: $STACK_NETWORK_NAME"
waf_compose up -d

for _ in $(seq 1 30); do
  if curl -ksS -o /dev/null "https://127.0.0.1:${WAF_PORT}/healthz"; then
    break
  fi
  sleep 1
done

log "Protected entrypoint:"
log "  https://127.0.0.1:$WAF_PORT"
