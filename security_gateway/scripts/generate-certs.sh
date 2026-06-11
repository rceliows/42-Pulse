#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

ensure_runtime_dirs

generate_ca() {
  local cert_dir="$1"
  local name="$2"

  if [[ -f "$cert_dir/ca.crt" && -f "$cert_dir/ca.key" ]]; then
    return 0
  fi

  log "Generating $name local CA..."
  openssl req -x509 -newkey rsa:4096 -sha256 -days 3650 -nodes \
    -subj "/CN=security_gateway-$name-local-ca" \
    -keyout "$cert_dir/ca.key" \
    -out "$cert_dir/ca.crt" >/dev/null 2>&1
  chmod 600 "$cert_dir/ca.key"
}

generate_signed_cert() {
  local cert_dir="$1"
  local name="$2"
  local common_name="$3"
  local san="$4"

  if [[ -f "$cert_dir/$name.crt" && -f "$cert_dir/$name.key" ]]; then
    return 0
  fi

  local tmp_conf csr_file
  tmp_conf="$(mktemp)"
  csr_file="$(mktemp)"

  cat > "$tmp_conf" <<EOF
[req]
distinguished_name=req
req_extensions=v3_req
prompt=no

[v3_req]
subjectAltName=$san

[v3_ext]
subjectAltName=$san
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
EOF

  log "Generating $name TLS certificate..."
  openssl req -newkey rsa:2048 -nodes \
    -subj "/CN=$common_name" \
    -keyout "$cert_dir/$name.key" \
    -out "$csr_file" \
    -config "$tmp_conf" >/dev/null 2>&1

  openssl x509 -req -sha256 -days 825 \
    -in "$csr_file" \
    -CA "$cert_dir/ca.crt" \
    -CAkey "$cert_dir/ca.key" \
    -CAcreateserial \
    -out "$cert_dir/$name.crt" \
    -extensions v3_ext \
    -extfile "$tmp_conf" >/dev/null 2>&1

  chmod 600 "$cert_dir/$name.key"
  rm -f "$tmp_conf" "$csr_file"
}

generate_ca "$SECURITY_GATEWAY_DIR/vault/certs" "vault"
generate_signed_cert "$SECURITY_GATEWAY_DIR/vault/certs" "vault" "security_gateway-vault" "DNS:localhost,DNS:vault,IP:127.0.0.1"

generate_ca "$SECURITY_GATEWAY_DIR/waf/certs" "waf"
generate_signed_cert "$SECURITY_GATEWAY_DIR/waf/certs" "waf" "localhost" "DNS:localhost,DNS:security_gateway-waf,IP:127.0.0.1"
chmod 644 "$SECURITY_GATEWAY_DIR/waf/certs/waf.key"

log "Certificates are ready."
