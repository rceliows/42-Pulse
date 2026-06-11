#!/usr/bin/env bash
set -euo pipefail

require_var() {
  local name="$1"
  if [[ -z "${!name:-}" ]]; then
    echo "ERROR: missing required variable: $name" >&2
    exit 1
  fi
}

get_config_value() {
  local key="$1"
  awk -F= -v k="$key" '
    $0 ~ "^[[:space:]]*" k "[[:space:]]*=" {
      v = substr($0, index($0, "=") + 1)
    }
    END {
      sub(/[[:space:]]*#.*/, "", v)
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", v)
      gsub(/^"|"$/, "", v)
      print v
    }
  ' "$CONFIG_FILE"
}

require_var "CONFIG_FILE"

if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "❌ $CONFIG_FILE not found"
  echo "   Create it with: cp repo/transcendance.config.example ../transcendance.config"
  exit 1
fi

config_dir="$(cd "$(dirname "$CONFIG_FILE")" && pwd -P)"
repo_root_abs=""
if [[ -n "${REPO_ROOT:-}" ]]; then
  repo_root_abs="$(cd "$REPO_ROOT" 2>/dev/null && pwd -P || true)"
fi

root_dir_raw="$(get_config_value "ROOT_DIR")"
if [[ -z "$root_dir_raw" ]]; then
  if [[ -n "$repo_root_abs" ]]; then
    root_dir="$repo_root_abs"
    root_dir_raw="(default from Make REPO_ROOT)"
  else
    echo "❌ ROOT_DIR is missing in $CONFIG_FILE and REPO_ROOT is not set"
    echo "   Add ROOT_DIR=repo in config, or run through Make (which provides REPO_ROOT)."
    exit 1
  fi
fi

if [[ "$root_dir_raw" != "(default from Make REPO_ROOT)" ]]; then
  if [[ "$root_dir_raw" = /* ]]; then
    root_dir="$root_dir_raw"
  else
    root_dir="$config_dir/$root_dir_raw"
  fi
  root_dir="$(cd "$root_dir" 2>/dev/null && pwd -P || true)"
fi

if [[ -z "$root_dir" || ! -d "$root_dir" ]]; then
  echo "❌ ROOT_DIR path is invalid in $CONFIG_FILE (got '$root_dir_raw')"
  exit 1
fi

if [[ ! -f "$root_dir/infra/docker-compose.yml" || ! -d "$root_dir/app" ]]; then
  echo "❌ ROOT_DIR must point to the repo root containing infra/docker-compose.yml and app/"
  echo "   Current ROOT_DIR resolves to: $root_dir"
  exit 1
fi

if [[ -n "$repo_root_abs" && "$root_dir" != "$repo_root_abs" ]]; then
  echo "❌ ROOT_DIR mismatch:"
  echo "   transcendance.config ROOT_DIR -> $root_dir"
  echo "   current Make repo root       -> $repo_root_abs"
  exit 1
fi

campus_id="$(get_config_value "CAMPUS_ID")"
if [[ -z "$campus_id" ]]; then
  echo "❌ CAMPUS_ID is missing in $CONFIG_FILE"
  exit 1
fi

case "$campus_id" in
  *[!0-9]*)
    echo "❌ CAMPUS_ID must be numeric in $CONFIG_FILE (got '$campus_id')"
    exit 1
    ;;
esac

db_port="$(get_config_value "DB_PORT")"
if [[ -z "$db_port" ]]; then
  echo "❌ DB_PORT is missing in $CONFIG_FILE"
  exit 1
fi
case "$db_port" in
  *[!0-9]*)
    echo "❌ DB_PORT must be numeric in $CONFIG_FILE (got '$db_port')"
    exit 1
    ;;
esac

db_name="$(get_config_value "DB_NAME")"
if [[ -z "$db_name" ]]; then
  echo "❌ DB_NAME is missing in $CONFIG_FILE"
  exit 1
fi

db_user="$(get_config_value "DB_USER")"
if [[ -z "$db_user" ]]; then
  echo "❌ DB_USER is missing in $CONFIG_FILE"
  exit 1
fi

db_password="$(get_config_value "DB_PASSWORD")"
if [[ -z "$db_password" ]]; then
  echo "❌ DB_PASSWORD is missing in $CONFIG_FILE"
  exit 1
fi

db_data_dir="$(get_config_value "DB_DATA_DIR")"
if [[ -z "$db_data_dir" ]]; then
  echo "❌ DB_DATA_DIR is missing in $CONFIG_FILE"
  exit 1
fi
if [[ "$db_data_dir" = "/" ]]; then
  echo "❌ DB_DATA_DIR cannot be '/' in $CONFIG_FILE"
  exit 1
fi

cli_campus_id="${CLI_CAMPUS_ID:-}"
if [[ -n "$cli_campus_id" ]]; then
  echo "❌ CAMPUS_ID override via CLI is forbidden."
  echo "   Remove CAMPUS_ID from command line and set CAMPUS_ID in $CONFIG_FILE."
  echo "   Example invalid: make deploy CAMPUS_ID=999"
  exit 1
fi

echo "✅ Runtime config validated"
