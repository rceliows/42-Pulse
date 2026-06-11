#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

require_vault_init_artifacts
load_current_stack_env

root_token="$(vault_root_token)"

: "${API_ROOT:?API_ROOT missing from $ENV_FILE}"
: "${CLIENT_ID:?CLIENT_ID missing from $ENV_FILE}"
: "${CLIENT_SECRET:?CLIENT_SECRET missing from $ENV_FILE}"
: "${REDIRECT_URI:?REDIRECT_URI missing from $ENV_FILE}"
: "${SCOPE:?SCOPE missing from $ENV_FILE}"
: "${DB_PASSWORD:?DB_PASSWORD missing from $CONFIG_FILE}"
: "${DB_USER:?DB_USER missing from $CONFIG_FILE}"
: "${DB_NAME:?DB_NAME missing from $CONFIG_FILE}"
: "${DB_PORT:?DB_PORT missing from $CONFIG_FILE}"
: "${CAMPUS_ID:?CAMPUS_ID missing from $CONFIG_FILE}"

DB_HOST="${DB_HOST:-db}"
WEB_PORT="${WEB_PORT:-8050}"
DB_DATA_DIR="${DB_DATA_DIR:-$ENV_ROOT/data/postgres}"
USER_DATA_STALE_AFTER_S="${USER_DATA_STALE_AFTER_S:-600}"
AUTO_MAINTENANCE_INTERVAL_S="${AUTO_MAINTENANCE_INTERVAL_S:-3600}"
AUTO_MAINTENANCE_REMOVE_ORPHANS="${AUTO_MAINTENANCE_REMOVE_ORPHANS:-0}"
ADMIN_API42_HEALTH_CACHE_S="${ADMIN_API42_HEALTH_CACHE_S:-300}"
HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX="${HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX:-200}"
HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX="${HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX:-1000}"
HEALTH_DEGRADED_PROCESS_QUEUE_MAX="${HEALTH_DEGRADED_PROCESS_QUEUE_MAX:-200}"

log "Seeding OAuth secrets into Vault..."
vault_exec_with_token "$root_token" kv put secret/transcendence/oauth \
  API_ROOT="$API_ROOT" \
  CLIENT_ID="$CLIENT_ID" \
  CLIENT_SECRET="$CLIENT_SECRET" \
  CLIENT_SECRET_NEXT="${CLIENT_SECRET_NEXT:-}" \
  REDIRECT_URI="$REDIRECT_URI" \
  SCOPE="$SCOPE" >/dev/null

log "Seeding database secrets into Vault..."
vault_exec_with_token "$root_token" kv put secret/transcendence/database \
  DB_HOST="$DB_HOST" \
  DB_PORT="$DB_PORT" \
  DB_NAME="$DB_NAME" \
  DB_USER="$DB_USER" \
  DB_PASSWORD="$DB_PASSWORD" \
  DB_DATA_DIR="$DB_DATA_DIR" >/dev/null

log "Seeding runtime configuration into Vault..."
vault_exec_with_token "$root_token" kv put secret/transcendence/runtime \
  CAMPUS_ID="$CAMPUS_ID" \
  WEB_PORT="$WEB_PORT" \
  USER_DATA_STALE_AFTER_S="$USER_DATA_STALE_AFTER_S" \
  AUTO_MAINTENANCE_INTERVAL_S="$AUTO_MAINTENANCE_INTERVAL_S" \
  AUTO_MAINTENANCE_REMOVE_ORPHANS="$AUTO_MAINTENANCE_REMOVE_ORPHANS" \
  ADMIN_API42_HEALTH_CACHE_S="$ADMIN_API42_HEALTH_CACHE_S" \
  HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX="$HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX" \
  HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX="$HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX" \
  HEALTH_DEGRADED_PROCESS_QUEUE_MAX="$HEALTH_DEGRADED_PROCESS_QUEUE_MAX" >/dev/null

log "Current stack values were copied into Vault."
