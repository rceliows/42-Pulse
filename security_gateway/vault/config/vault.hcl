ui = true
disable_mlock = true

storage "raft" {
  path    = "/vault/data"
  node_id = "security_gateway-vault"
}

listener "tcp" {
  address       = "0.0.0.0:8200"
  tls_cert_file = "/vault/certs/vault.crt"
  tls_key_file  = "/vault/certs/vault.key"
}

api_addr = "https://127.0.0.1:8200"
cluster_addr = "https://127.0.0.1:8201"

log_level = "info"
