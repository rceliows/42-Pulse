ui = true
disable_mlock = true
cluster_name = "transcendence-security"
api_addr = "https://vault:8200"
cluster_addr = "https://vault:8201"

listener "tcp" {
  address = "0.0.0.0:8200"
  cluster_address = "0.0.0.0:8201"
  tls_disable = 0
  tls_cert_file = "/vault/certs/vault.crt"
  tls_key_file = "/vault/certs/vault.key"
}

storage "raft" {
  path = "/vault/data"
  node_id = "transcendence-vault-1"
}

telemetry {
  disable_hostname = true
  prometheus_retention_time = "24h"
}
