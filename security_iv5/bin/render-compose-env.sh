#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

token="$(approle_login_token)"
tmp_file="$(mktemp)"
cleanup() {
  rm -f "$tmp_file"
  vault_exec_with_token "$token" token revoke -self >/dev/null 2>&1 || true
}
trap cleanup EXIT

oauth_json="$(vault_exec_with_token "$token" kv get -format=json secret/transcendence/oauth | jq -c '.data.data')"
db_json="$(vault_exec_with_token "$token" kv get -format=json secret/transcendence/database | jq -c '.data.data')"
runtime_json="$(vault_exec_with_token "$token" kv get -format=json secret/transcendence/runtime | jq -c '.data.data')"

{
  printf '# generated from Vault - source this file locally if needed\n'
  shell_assign "API_ROOT" "$(jq -r '.API_ROOT // ""' <<<"$oauth_json")"
  shell_assign "CLIENT_ID" "$(jq -r '.CLIENT_ID // ""' <<<"$oauth_json")"
  shell_assign "CLIENT_SECRET" "$(jq -r '.CLIENT_SECRET // ""' <<<"$oauth_json")"
  shell_assign "CLIENT_SECRET_NEXT" "$(jq -r '.CLIENT_SECRET_NEXT // ""' <<<"$oauth_json")"
  shell_assign "REDIRECT_URI" "$(jq -r '.REDIRECT_URI // ""' <<<"$oauth_json")"
  shell_assign "SCOPE" "$(jq -r '.SCOPE // ""' <<<"$oauth_json")"
  shell_assign "DB_HOST" "$(jq -r '.DB_HOST // ""' <<<"$db_json")"
  shell_assign "DB_PORT" "$(jq -r '.DB_PORT // ""' <<<"$db_json")"
  shell_assign "DB_NAME" "$(jq -r '.DB_NAME // ""' <<<"$db_json")"
  shell_assign "DB_USER" "$(jq -r '.DB_USER // ""' <<<"$db_json")"
  shell_assign "DB_PASSWORD" "$(jq -r '.DB_PASSWORD // ""' <<<"$db_json")"
  shell_assign "DB_DATA_DIR" "$(jq -r '.DB_DATA_DIR // ""' <<<"$db_json")"
  shell_assign "CAMPUS_ID" "$(jq -r '.CAMPUS_ID // ""' <<<"$runtime_json")"
  shell_assign "WEB_PORT" "$(jq -r '.WEB_PORT // ""' <<<"$runtime_json")"
  shell_assign "USER_DATA_STALE_AFTER_S" "$(jq -r '.USER_DATA_STALE_AFTER_S // ""' <<<"$runtime_json")"
  shell_assign "AUTO_MAINTENANCE_INTERVAL_S" "$(jq -r '.AUTO_MAINTENANCE_INTERVAL_S // ""' <<<"$runtime_json")"
  shell_assign "AUTO_MAINTENANCE_REMOVE_ORPHANS" "$(jq -r '.AUTO_MAINTENANCE_REMOVE_ORPHANS // ""' <<<"$runtime_json")"
  shell_assign "ADMIN_API42_HEALTH_CACHE_S" "$(jq -r '.ADMIN_API42_HEALTH_CACHE_S // ""' <<<"$runtime_json")"
  shell_assign "HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX" "$(jq -r '.HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX // ""' <<<"$runtime_json")"
  shell_assign "HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX" "$(jq -r '.HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX // ""' <<<"$runtime_json")"
  shell_assign "HEALTH_DEGRADED_PROCESS_QUEUE_MAX" "$(jq -r '.HEALTH_DEGRADED_PROCESS_QUEUE_MAX // ""' <<<"$runtime_json")"
} > "$tmp_file"

mv "$tmp_file" "$VAULT_RENDERED_ENV_FILE"
chmod 600 "$VAULT_RENDERED_ENV_FILE"

log "Rendered Vault env file:"
log "  $VAULT_RENDERED_ENV_FILE"
