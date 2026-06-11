#!/usr/bin/env bash
set -euo pipefail

require_var() {
  local name="$1"
  if [[ -z "${!name:-}" ]]; then
    echo "ERROR: missing required variable: $name" >&2
    exit 1
  fi
}

require_var "ENV_FILE"

if [[ ! -f "$ENV_FILE" ]]; then
  echo "❌ $ENV_FILE not found"
  echo "   Create it with: cp repo/.env.example ../.env"
  exit 1
fi

echo "✅ API environment file detected"
