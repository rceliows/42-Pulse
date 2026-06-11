#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/common.sh"

ensure_runtime_dirs

VAULT_CERT_DIR="$SECURITY_DIR/vault/certs"
WAF_CERT_DIR="$SECURITY_DIR/waf/certs"
CA_KEY="$VAULT_CERT_DIR/ca.key"
CA_CERT="$VAULT_CERT_DIR/ca.crt"

generate_leaf() {
  local common_name="$1"
  local out_dir="$2"
  local dns_names="$3"
  local ip_names="$4"
  local key_file="$out_dir/$common_name.key"
  local csr_file="$out_dir/$common_name.csr"
  local crt_file="$out_dir/$common_name.crt"
  local ext_file="$out_dir/$common_name.ext"

  openssl genrsa -out "$key_file" 4096 >/dev/null 2>&1
  openssl req -new -key "$key_file" -out "$csr_file" -subj "/CN=$common_name" >/dev/null 2>&1

  {
    printf 'basicConstraints=CA:FALSE\n'
    printf 'keyUsage=digitalSignature,keyEncipherment\n'
    printf 'extendedKeyUsage=serverAuth\n'
    printf 'subjectAltName=%s,%s\n' "$dns_names" "$ip_names"
  } > "$ext_file"

  openssl x509 \
    -req \
    -in "$csr_file" \
    -CA "$CA_CERT" \
    -CAkey "$CA_KEY" \
    -CAcreateserial \
    -out "$crt_file" \
    -days 825 \
    -sha256 \
    -extfile "$ext_file" >/dev/null 2>&1

  rm -f "$csr_file" "$ext_file"
  chmod 600 "$key_file"
  chmod 644 "$crt_file"
}

if [[ ! -f "$CA_KEY" || ! -f "$CA_CERT" ]]; then
  log "Generating local security CA..."
  openssl genrsa -out "$CA_KEY" 4096 >/dev/null 2>&1
  openssl req \
    -x509 \
    -new \
    -nodes \
    -key "$CA_KEY" \
    -sha256 \
    -days 3650 \
    -out "$CA_CERT" \
    -subj "/CN=Transcendence Security CA" >/dev/null 2>&1
  chmod 600 "$CA_KEY"
  chmod 644 "$CA_CERT"
fi

log "Generating Vault certificate..."
generate_leaf "vault" "$VAULT_CERT_DIR" "DNS:vault,DNS:localhost" "IP:127.0.0.1"

log "Generating WAF certificate..."
generate_leaf "waf" "$WAF_CERT_DIR" "DNS:waf,DNS:localhost" "IP:127.0.0.1"
chmod 644 "$WAF_CERT_DIR/waf.key"

cp "$CA_CERT" "$WAF_CERT_DIR/ca.crt"
chmod 644 "$WAF_CERT_DIR/ca.crt"

log "Certificates ready in:"
log "  $VAULT_CERT_DIR"
log "  $WAF_CERT_DIR"
