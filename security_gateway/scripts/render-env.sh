#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

require_vault_init_artifacts

token="$(approle_login_token)"
tmp_file="$(mktemp)"

vault_exec_with_token "$token" kv get -format=json secret/security_gateway/oauth \
  | jq -r '
      .data.data
      | to_entries[]
      | select(.value != null)
      | "\(.key)=\(.value | @sh)"
    ' > "$tmp_file"

{
  printf 'NODE_ENV=production\n'
  cat "$tmp_file"
} > "$RENDERED_OAUTH_ENV_FILE"

rm -f "$tmp_file"
chmod 600 "$RENDERED_OAUTH_ENV_FILE"

log "Rendered OAuth gateway env file from Vault:"
log "  $RENDERED_OAUTH_ENV_FILE"
