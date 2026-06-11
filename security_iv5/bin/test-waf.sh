#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

base_url="https://127.0.0.1:${WAF_PORT}"

request_code() {
  local method="$1"
  local url="$2"
  shift 2
  curl -ksS -o /dev/null -w '%{http_code}' -X "$method" "$url" "$@"
}

healthy="$(request_code GET "$base_url/healthz")"
sensitive_block="$(request_code GET "$base_url/.env")"
attack_block="$(request_code GET "$base_url/?q=%3Cscript%3Ealert(1)%3C/script%3E")"
bad_method="$(request_code PUT "$base_url/api/events/latest")"

printf 'healthz=%s\n' "$healthy"
printf 'sensitive_probe=%s\n' "$sensitive_block"
printf 'xss_probe=%s\n' "$attack_block"
printf 'bad_method=%s\n' "$bad_method"

[[ "$healthy" == "200" ]] || fail "Expected /healthz to return 200"
[[ "$sensitive_block" == "403" ]] || fail "Expected /.env to be blocked with 403"
[[ "$attack_block" == "403" ]] || fail "Expected obvious attack payload to be blocked with 403"
[[ "$bad_method" == "405" || "$bad_method" == "403" ]] || fail "Expected unsupported method to be blocked"

log "WAF smoke tests passed."
