#!/usr/bin/env bash
set -euo pipefail

require_var() {
  local name="$1"
  if [[ -z "${!name:-}" ]]; then
    echo "ERROR: missing required variable: $name" >&2
    exit 1
  fi
}

require_var "ROOT_DIR"

root_dir="$(cd "$ROOT_DIR" 2>/dev/null && pwd -P || true)"
if [[ -z "$root_dir" || ! -d "$root_dir" ]]; then
  echo "❌ ROOT_DIR is invalid: '$ROOT_DIR'" >&2
  exit 1
fi

base_dir="$(cd "$root_dir/.." && pwd -P)"
runtime_dir="$base_dir/runtime"
data_dir="$base_dir/data"
backups_dir="$base_dir/backups"
db_data_dir="$data_dir/postgres"
oauth_state_file="$root_dir/.oauth_state"
env_file="$base_dir/.env"

if [[ -d "$runtime_dir" ]]; then
  echo "⚠️  runtime already present at $runtime_dir (keeping existing files)"
fi

created_count=0

ensure_dir() {
  local dir="$1"
  if [[ ! -d "$dir" ]]; then
    mkdir -p "$dir"
    echo "➕ created: $dir"
    created_count=$((created_count + 1))
  fi
}

for d in \
  "$runtime_dir" \
  "$runtime_dir/backlog" \
  "$runtime_dir/cache" \
  "$runtime_dir/cache/bin" \
  "$runtime_dir/cache/raw_detect" \
  "$runtime_dir/cleanup" \
  "$runtime_dir/exports" \
  "$runtime_dir/logs" \
  "$runtime_dir/logs/agents" \
  "$runtime_dir/logs/archive" \
  "$runtime_dir/logs/archive/health" \
  "$runtime_dir/logs/ops" \
  "$runtime_dir/logs/pids" \
  "$runtime_dir/logs/state" \
  "$data_dir" \
  "$db_data_dir" \
  "$backups_dir"; do
  ensure_dir "$d"
done

extract_env_value() {
  local key="$1"
  local file="$2"
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
  ' "$file"
}

if [[ ! -f "$oauth_state_file" ]]; then
  access_token=""
  refresh_token=""
  if [[ -f "$env_file" ]]; then
    access_token="$(extract_env_value "ACCESS_TOKEN" "$env_file")"
    refresh_token="$(extract_env_value "REFRESH_TOKEN" "$env_file")"
  fi

  if [[ -n "$access_token" || -n "$refresh_token" ]]; then
    {
      [[ -n "$access_token" ]] && printf 'ACCESS_TOKEN=%s\n' "$access_token"
      [[ -n "$refresh_token" ]] && printf 'REFRESH_TOKEN=%s\n' "$refresh_token"
    } > "$oauth_state_file"
    chmod 600 "$oauth_state_file"
    echo "➕ created: $oauth_state_file (bootstrapped from ../.env)"
    created_count=$((created_count + 1))
  else
    {
      echo "# OAuth runtime state"
      echo "# Populate with: make exchange CODE=\"<AUTHORIZATION_CODE>\""
      echo "ACCESS_TOKEN="
      echo "REFRESH_TOKEN="
    } > "$oauth_state_file"
    chmod 600 "$oauth_state_file"
    echo "➕ created: $oauth_state_file (placeholder)"
    created_count=$((created_count + 1))
  fi
fi

if [[ "$created_count" -eq 0 ]]; then
  echo "✅ runtime/data/backups already ready (no folder created)"
else
  echo "✅ runtime/data/backups ready ($created_count folder(s) created)"
fi
