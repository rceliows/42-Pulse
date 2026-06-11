path "secret/data/transcendence/*" {
  capabilities = ["read"]
}

path "secret/metadata/transcendence/*" {
  capabilities = ["read", "list"]
}

path "auth/token/lookup-self" {
  capabilities = ["read"]
}

path "auth/token/renew-self" {
  capabilities = ["update"]
}
