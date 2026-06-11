path "secret/data/security_gateway/*" {
  capabilities = ["read"]
}

path "secret/metadata/security_gateway/*" {
  capabilities = ["read", "list"]
}
