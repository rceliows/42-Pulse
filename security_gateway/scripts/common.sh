#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SECURITY_GATEWAY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$SECURITY_GATEWAY_DIR/.." && pwd)"

NORMALIZED_PROJECT_NAME="$(
  basename "$REPO_ROOT" \
    | tr '[:upper:]' '[:lower:]' \
    | sed -E 's/[^a-z0-9]+/_/g; s/^_+//; s/_+$//'
)"

COMPOSE_FILE="$SECURITY_GATEWAY_DIR/docker-compose.yml"
PROJECT_NAME="${SECURITY_GATEWAY_PROJECT_NAME:-${NORMALIZED_PROJECT_NAME}_security_gateway}"
STACK_NETWORK_NAME="${STACK_NETWORK_NAME:-${NORMALIZED_PROJECT_NAME}_transcendence}"
VAULT_PORT="${VAULT_PORT:-18200}"
WAF_PORT="${WAF_PORT:-9443}"

VAULT_ADDR_LOCAL="https://127.0.0.1:8200"
VAULT_CACERT_CONTAINER="/vault/certs/ca.crt"
VAULT_INIT_FILE="$SECURITY_GATEWAY_DIR/runtime/bootstrap/vault-init.json"
APPROLE_ROLE_ID_FILE="$SECURITY_GATEWAY_DIR/runtime/approle/role-id"
APPROLE_SECRET_ID_FILE="$SECURITY_GATEWAY_DIR/runtime/approle/secret-id"
RENDERED_OAUTH_ENV_FILE="$SECURITY_GATEWAY_DIR/runtime/rendered/oauth-gateway.env"
LOCAL_OAUTH_ENV_FILE="$SECURITY_GATEWAY_DIR/local/oauth.env"

log() {
  printf '%s\n' "$*"
}

fail() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

ensure_runtime_dirs() {
  mkdir -p \
    "$SECURITY_GATEWAY_DIR/runtime/bootstrap" \
    "$SECURITY_GATEWAY_DIR/runtime/approle" \
    "$SECURITY_GATEWAY_DIR/runtime/rendered" \
    "$SECURITY_GATEWAY_DIR/runtime/logs/vault" \
    "$SECURITY_GATEWAY_DIR/runtime/logs/waf" \
    "$SECURITY_GATEWAY_DIR/vault/data" \
    "$SECURITY_GATEWAY_DIR/vault/certs" \
    "$SECURITY_GATEWAY_DIR/waf/certs"

  touch "$RENDERED_OAUTH_ENV_FILE"
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
  export SECURITY_GATEWAY_ROOT="$SECURITY_GATEWAY_DIR"
  export HOST_UID="${HOST_UID:-$(id -u)}"
  export HOST_GID="${HOST_GID:-$(id -g)}"
  export STACK_NETWORK_NAME
  export VAULT_PORT
  export WAF_PORT
}

compose() {
  export_context
  docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" "$@"
}

vault_exec() {
  compose exec -T \
    -e VAULT_ADDR="$VAULT_ADDR_LOCAL" \
    -e VAULT_CACERT="$VAULT_CACERT_CONTAINER" \
    vault vault "$@"
}

vault_exec_with_token() {
  local token="$1"
  shift
  compose exec -T \
    -e VAULT_ADDR="$VAULT_ADDR_LOCAL" \
    -e VAULT_CACERT="$VAULT_CACERT_CONTAINER" \
    -e VAULT_TOKEN="$token" \
    vault vault "$@"
}

vault_status_json() {
  compose exec -T \
    -e VAULT_ADDR="$VAULT_ADDR_LOCAL" \
    -e VAULT_CACERT="$VAULT_CACERT_CONTAINER" \
    vault sh -lc 'vault status -format=json 2>/dev/null || true'
}

wait_for_vault() {
  local status_json initialized
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

load_local_oauth_env() {
  if [[ -f "$LOCAL_OAUTH_ENV_FILE" ]]; then
    set -a
    # shellcheck disable=SC1090
    source "$LOCAL_OAUTH_ENV_FILE"
    set +a
  fi
}

ensure_stack_network_or_die() {
  detect_stack_network || fail "Could not find the existing project Docker network. Start the current stack first."
}

show_status() {
  export_context
  compose ps || true
}

show_logs() {
  export_context
  compose logs --tail=200
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  case "${1-}" in
    status) show_status ;;
    logs) show_logs ;;
  esac
fi
