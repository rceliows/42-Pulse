#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

require_vault_init_artifacts
load_local_oauth_env

root_token="$(vault_root_token)"

generated_session_secret="0"
if [[ -z "${SESSION_SECRET:-}" ]]; then
  SESSION_SECRET="$(openssl rand -base64 48)"
  generated_session_secret="1"
fi

log "Seeding simple 42 OAuth secrets into Vault path secret/security_gateway/oauth..."
vault_exec_with_token "$root_token" kv put secret/security_gateway/oauth \
  PUBLIC_BASE_URL="${PUBLIC_BASE_URL:-https://localhost:${WAF_PORT}}" \
  UPSTREAM_URL="${UPSTREAM_URL:-http://web:80}" \
  SESSION_SECRET="$SESSION_SECRET" \
  SESSION_TTL_SECONDS="${SESSION_TTL_SECONDS:-28800}" \
  OAUTH_42_CLIENT_ID="${OAUTH_42_CLIENT_ID:-}" \
  OAUTH_42_CLIENT_SECRET="${OAUTH_42_CLIENT_SECRET:-}" \
  OAUTH_42_SCOPES="${OAUTH_42_SCOPES:-public}" >/dev/null

if [[ "$generated_session_secret" == "1" ]]; then
  log "Generated SESSION_SECRET directly in Vault because local/oauth.env did not provide one."
fi

log "Vault seed complete. Real OAuth client secrets were not printed."
