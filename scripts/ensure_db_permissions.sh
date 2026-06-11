#!/usr/bin/env bash
set -euo pipefail

require_var() {
  local name="$1"
  if [[ -z "${!name:-}" ]]; then
    echo "ERROR: missing required variable: $name" >&2
    exit 1
  fi
}

require_var "DB_DATA_DIR"
require_var "DB_CONTAINER_UID"
require_var "DB_CONTAINER_GID"

db_data_dir="$DB_DATA_DIR"
mkdir -p "$db_data_dir"

current_uid="$(stat -c '%u' "$db_data_dir" 2>/dev/null || true)"
current_gid="$(stat -c '%g' "$db_data_dir" 2>/dev/null || true)"
current_mode="$(stat -c '%a' "$db_data_dir" 2>/dev/null || true)"

if [[ "$current_uid" == "$DB_CONTAINER_UID" && "$current_gid" == "$DB_CONTAINER_GID" && "$current_mode" == "700" ]]; then
  echo "✅ PostgreSQL data dir ready ($db_data_dir owner=${current_uid}:${current_gid} mode=$current_mode)"
  exit 0
fi

echo "🔐 Aligning PostgreSQL data dir ownership ($db_data_dir -> ${DB_CONTAINER_UID}:${DB_CONTAINER_GID}, mode 700)"
if ! docker run --rm \
  -v "$db_data_dir:/var/lib/postgresql/data" \
  --entrypoint sh \
  postgres:16 \
  -c "mkdir -p /var/lib/postgresql/data && chown -R ${DB_CONTAINER_UID}:${DB_CONTAINER_GID} /var/lib/postgresql/data && chmod 700 /var/lib/postgresql/data"; then
  echo "❌ Failed to align PostgreSQL data dir ownership" >&2
  echo "   Path: $db_data_dir" >&2
  echo "   Expected owner: ${DB_CONTAINER_UID}:${DB_CONTAINER_GID}" >&2
  exit 1
fi

new_state="$(stat -c '%u:%g mode=%a' "$db_data_dir" 2>/dev/null || true)"
echo "✅ PostgreSQL data dir ready ($db_data_dir $new_state)"
