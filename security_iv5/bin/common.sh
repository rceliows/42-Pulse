#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SECURITY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$SECURITY_DIR/.." && pwd)"
ENV_ROOT="$(cd "$REPO_ROOT/.." && pwd)"

ENV_FILE="${ENV_FILE:-$ENV_ROOT/.env}"
CONFIG_FILE="${CONFIG_FILE:-$ENV_ROOT/transcendance.config}"

NORMALIZED_PROJECT_NAME="$(
  basename "$REPO_ROOT" \
    | tr '[:upper:]' '[:lower:]' \
    | sed -E 's/[^a-z0-9]+/_/g; s/^_+//; s/_+$//'
)"

SECURITY_ROOT="$SECURITY_DIR"
VAULT_COMPOSE_FILE="$SECURITY_DIR/docker-compose.vault.yml"
WAF_COMPOSE_FILE="$SECURITY_DIR/docker-compose.waf.yml"
VAULT_PROJECT_NAME="${VAULT_PROJECT_NAME:-${NORMALIZED_PROJECT_NAME}_security_vault}"
WAF_PROJECT_NAME="${WAF_PROJECT_NAME:-${NORMALIZED_PROJECT_NAME}_security_waf}"
STACK_NETWORK_NAME="${STACK_NETWORK_NAME:-${NORMALIZED_PROJECT_NAME}_transcendence}"
VAULT_PORT="${VAULT_PORT:-8200}"
WAF_PORT="${WAF_PORT:-8443}"

VAULT_INIT_FILE="$SECURITY_DIR/runtime/bootstrap/vault-init.json"
APPROLE_ROLE_ID_FILE="$SECURITY_DIR/runtime/approle/role-id"
APPROLE_SECRET_ID_FILE="$SECURITY_DIR/runtime/approle/secret-id"
VAULT_RENDERED_ENV_FILE="$SECURITY_DIR/runtime/rendered/transcendence.vault.env"
VAULT_ADDR_LOCAL="https://127.0.0.1:8200"
VAULT_CACERT_CONTAINER="/vault/certs/ca.crt"

log() {
  printf '%s\n' "$*"
}

fail() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

ensure_runtime_dirs() {
  local dir

  mkdir -p \
    "$SECURITY_DIR/runtime/approle" \
    "$SECURITY_DIR/runtime/bootstrap" \
    "$SECURITY_DIR/runtime/rendered" \
    "$SECURITY_DIR/runtime/logs/vault" \
    "$SECURITY_DIR/runtime/logs/waf" \
    "$SECURITY_DIR/vault/data" \
    "$SECURITY_DIR/vault/certs" \
    "$SECURITY_DIR/waf/certs"

  for dir in \
    "$SECURITY_DIR/runtime/logs/vault" \
    "$SECURITY_DIR/runtime/logs/waf"
  do
    if [[ ! -w "$dir" ]]; then
      rm -rf "$dir"
      mkdir -p "$dir"
    fi
  done
}

load_shell_file() {
  local file="$1"
  if [[ -f "$file" ]]; then
    set -a
    # shellcheck disable=SC1090
    source "$file"
    set +a
  fi
}

load_current_stack_env() {
  load_shell_file "$ENV_FILE"
  load_shell_file "$CONFIG_FILE"
}

load_vault_rendered_env_if_present() {
  load_shell_file "$VAULT_RENDERED_ENV_FILE"
}

detect_stack_network() {
  local candidate
  for candidate in \
    "$STACK_NETWORK_NAME" \
    "transcendence" \
    "${NORMALIZED_PROJECT_NAME}-transcendence"
  do
    if docker network inspect "$candidate" >/dev/null 2>&1; then
      STACK_NETWORK_NAME="$candidate"
      export STACK_NETWORK_NAME
      return 0
    fi
  done

  candidate="$(docker network ls --format '{{.Name}}' | grep '_transcendence$' | head -n 1 || true)"
  if [[ -n "$candidate" ]]; then
    STACK_NETWORK_NAME="$candidate"
    export STACK_NETWORK_NAME
    return 0
  fi

  return 1
}

export_context() {
  ensure_runtime_dirs
  load_current_stack_env

  export HOST_UID="${HOST_UID:-$(id -u)}"
  export HOST_GID="${HOST_GID:-$(id -g)}"
  export SECURITY_ROOT
  export STACK_NETWORK_NAME
  export VAULT_PORT
  export WAF_PORT
}

vault_compose() {
  export_context
  docker compose -p "$VAULT_PROJECT_NAME" -f "$VAULT_COMPOSE_FILE" "$@"
}

waf_compose() {
  export_context
  docker compose -p "$WAF_PROJECT_NAME" -f "$WAF_COMPOSE_FILE" "$@"
}

vault_exec() {
  vault_compose exec -T \
    -e VAULT_ADDR="$VAULT_ADDR_LOCAL" \
    -e VAULT_CACERT="$VAULT_CACERT_CONTAINER" \
    vault vault "$@"
}

vault_exec_with_token() {
  local token="$1"
  shift
  vault_compose exec -T \
    -e VAULT_ADDR="$VAULT_ADDR_LOCAL" \
    -e VAULT_CACERT="$VAULT_CACERT_CONTAINER" \
    -e VAULT_TOKEN="$token" \
    vault vault "$@"
}

vault_status_json() {
  vault_compose exec -T \
    -e VAULT_ADDR="$VAULT_ADDR_LOCAL" \
    -e VAULT_CACERT="$VAULT_CACERT_CONTAINER" \
    vault sh -lc 'vault status -format=json 2>/dev/null || true'
}

wait_for_vault() {
  local status_json
  local initialized
  local attempts=60

  while (( attempts > 0 )); do
    status_json="$(vault_status_json)"
    initialized="$(jq -r 'if has("initialized") then (.initialized | tostring) else empty end' <<<"$status_json" 2>/dev/null || true)"
    if [[ "$initialized" == "true" || "$initialized" == "false" ]]; then
      return 0
    fi
    sleep 2
    ((attempts--))
  done

  fail "Vault did not become reachable in time."
}

require_file() {
  local file="$1"
  [[ -f "$file" ]] || fail "Missing required file: $file"
}

require_vault_init_artifacts() {
  require_file "$VAULT_INIT_FILE"
}

vault_root_token() {
  jq -r '.root_token' "$VAULT_INIT_FILE"
}

vault_unseal_key() {
  jq -r '.unseal_keys_b64[0]' "$VAULT_INIT_FILE"
}

approle_login_token() {
  require_file "$APPROLE_ROLE_ID_FILE"
  require_file "$APPROLE_SECRET_ID_FILE"

  local role_id secret_id login_json
  role_id="$(<"$APPROLE_ROLE_ID_FILE")"
  secret_id="$(<"$APPROLE_SECRET_ID_FILE")"

  login_json="$(
    vault_exec write -format=json auth/approle/login \
      role_id="$role_id" \
      secret_id="$secret_id"
  )"

  jq -r '.auth.client_token' <<<"$login_json"
}

shell_assign() {
  local key="$1"
  local value="${2-}"
  printf "%s=%q\n" "$key" "$value"
}

ensure_stack_network_or_die() {
  detect_stack_network || fail "Could not find the existing stack network. Start the current project stack first."
}

show_status() {
  export_context
  log "Vault compose:"
  vault_compose ps || true
  log ""
  if detect_stack_network; then
    log "WAF compose:"
    waf_compose ps || true
  else
    log "WAF compose: stack network not detected yet."
  fi
}

show_vault_logs() {
  export_context
  vault_compose logs --tail=200
}

show_waf_logs() {
  export_context
  ensure_stack_network_or_die
  waf_compose logs --tail=200
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  if [[ "${1-}" == "status" ]]; then
    show_status
  elif [[ "${1-}" == "vault-logs" ]]; then
    show_vault_logs
  elif [[ "${1-}" == "waf-logs" ]]; then
    show_waf_logs
  fi
fi
