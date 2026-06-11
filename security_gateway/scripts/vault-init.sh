#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

require_file "$SECURITY_GATEWAY_DIR/vault/config/vault.hcl"
require_file "$SECURITY_GATEWAY_DIR/vault/policies/security_gateway-runtime.hcl"
require_file "$SECURITY_GATEWAY_DIR/vault/certs/ca.crt"
require_file "$SECURITY_GATEWAY_DIR/vault/certs/vault.crt"
require_file "$SECURITY_GATEWAY_DIR/vault/certs/vault.key"

log "Starting security_gateway Vault..."
compose up -d vault
wait_for_vault

status_json="$(vault_status_json)"
initialized="$(jq -r 'if has("initialized") then (.initialized | tostring) else "false" end' <<<"$status_json")"

if [[ "$initialized" != "true" ]]; then
  log "Initializing Vault..."
  tmp_file="$(mktemp)"
  vault_exec operator init -key-shares=1 -key-threshold=1 -format=json > "$tmp_file"
  mv "$tmp_file" "$VAULT_INIT_FILE"
  chmod 600 "$VAULT_INIT_FILE"
else
  log "Vault already initialized."
  require_vault_init_artifacts
fi

status_json="$(vault_status_json)"
sealed="$(jq -r 'if has("sealed") then (.sealed | tostring) else "true" end' <<<"$status_json")"

if [[ "$sealed" == "true" ]]; then
  log "Unsealing Vault..."
  vault_exec operator unseal "$(vault_unseal_key)" >/dev/null
fi

root_token="$(vault_root_token)"

if ! vault_exec_with_token "$root_token" audit list -format=json | jq -e 'has("file/")' >/dev/null 2>&1; then
  log "Enabling Vault audit stream on stdout..."
  vault_exec_with_token "$root_token" audit enable file file_path=stdout >/dev/null
fi

if ! vault_exec_with_token "$root_token" secrets list -format=json | jq -e 'has("secret/")' >/dev/null 2>&1; then
  log "Enabling kv-v2 at secret/ ..."
  vault_exec_with_token "$root_token" secrets enable -path=secret -version=2 kv >/dev/null
fi

if ! vault_exec_with_token "$root_token" auth list -format=json | jq -e 'has("approle/")' >/dev/null 2>&1; then
  log "Enabling AppRole auth..."
  vault_exec_with_token "$root_token" auth enable approle >/dev/null
fi

log "Writing security_gateway runtime policy..."
vault_exec_with_token "$root_token" \
  policy write \
  security_gateway-runtime \
  /workdir/security_gateway/vault/policies/security_gateway-runtime.hcl >/dev/null

log "Configuring security_gateway AppRole..."
vault_exec_with_token "$root_token" write auth/approle/role/security_gateway-runtime \
  token_ttl=1h \
  token_max_ttl=4h \
  secret_id_ttl=168h \
  secret_id_num_uses=0 \
  token_num_uses=0 \
  token_policies=security_gateway-runtime >/dev/null

vault_exec_with_token "$root_token" read -format=json auth/approle/role/security_gateway-runtime/role-id \
  | jq -r '.data.role_id' > "$APPROLE_ROLE_ID_FILE"

vault_exec_with_token "$root_token" write -f -format=json auth/approle/role/security_gateway-runtime/secret-id \
  | jq -r '.data.secret_id' > "$APPROLE_SECRET_ID_FILE"

chmod 600 "$APPROLE_ROLE_ID_FILE" "$APPROLE_SECRET_ID_FILE"

log "Vault bootstrap complete."
log "  init file: $VAULT_INIT_FILE"
log "  AppRole:   $APPROLE_ROLE_ID_FILE and $APPROLE_SECRET_ID_FILE"
