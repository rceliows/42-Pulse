# IV.5 Cybersecurity

This folder adds the `IV.5 Cybersecurity` major module in a fully isolated way.

It does not modify the current project files. Instead, it adds:

- a hardened `ModSecurity + OWASP CRS` WAF in front of the existing `web` container,
- a dedicated `HashiCorp Vault` service with TLS and integrated storage,
- bootstrap scripts to migrate the current secrets into Vault,
- a renderer that turns Vault data into an ephemeral env file for future secure restarts,
- a separate `Makefile` so the team can operate this module without touching the root one.

## What It Connects To

Current stack entrypoints discovered in the existing project:

- `web` is the current public/static reverse-proxy container,
- `api` is the Express/WebSocket service,
- secrets currently live in sibling plaintext files:
  - `../.env`
  - `../transcendance.config`
  - `repo/.oauth_state`

Because the request was to not edit any existing file, this module keeps the live stack intact and layers security beside it.

## Folder Map

```text
security_iv5/
├── Makefile
├── docker-compose.vault.yml
├── docker-compose.waf.yml
├── bin/
│   ├── common.sh
│   ├── generate-certs.sh
│   ├── render-compose-env.sh
│   ├── seed-from-current-config.sh
│   ├── test-waf.sh
│   ├── vault-init.sh
│   ├── waf-down.sh
│   └── waf-up.sh
├── docs/
│   └── EXISTING_STACK_ANALYSIS.md
├── vault/
│   ├── config/vault.hcl
│   └── policies/transcendence-runtime.hcl
└── waf/
    ├── conf/
    │   ├── nginx.conf.template
    │   ├── conf.d/default.conf.template
    │   └── modsecurity.d/modsecurity-override.conf.template
    └── rules/
        └── REQUEST-900-LOCAL-HARDENING.conf
```

## Quick Start

From this folder:

```bash
make bootstrap
make waf-up
make test
```

Then use the protected entrypoint:

```text
https://127.0.0.1:8443
```

Vault UI:

```text
https://127.0.0.1:8200
```

## Workflow

1. `make bootstrap`
   - generates a local CA and service certificates,
   - starts Vault,
   - initializes and unseals Vault,
   - enables `kv-v2` and `approle`,
   - creates the `transcendence-runtime` policy,
   - seeds Vault from the current `../.env` and `../transcendance.config`,
   - renders `runtime/rendered/transcendence.vault.env`.

2. `make waf-up`
   - joins the current Docker network used by the existing stack,
   - starts a hardened WAF that proxies to `http://web:80`.

3. `make test`
   - checks healthy traffic,
   - checks that sensitive file probing is blocked,
   - checks that obvious attack patterns are blocked.

## Important Constraint

The current project still reads secrets from the original sibling files. Since the request explicitly forbade editing the existing repo files, this module does not rewrite the current app services to consume Vault directly.

What it does instead:

- Vault becomes the isolated encrypted source of truth,
- current secrets are migrated into Vault,
- a rendered env file is produced for secure relaunches or future CI/CD wiring,
- the WAF is fully operational immediately without changing the existing application code.

## Operations

```bash
make bootstrap   # full Vault bootstrap + seed + env render
make render      # refresh rendered env file from Vault
make waf-up      # start hardened WAF in front of the existing web service
make waf-down    # stop WAF only
make vault-logs  # inspect Vault logs
make waf-logs    # inspect WAF logs
make test        # run WAF smoke checks
```

## Generated Sensitive Files

These are intentionally ignored by `.gitignore` inside this folder:

- `runtime/bootstrap/vault-init.json`
- `runtime/approle/role-id`
- `runtime/approle/secret-id`
- `runtime/rendered/transcendence.vault.env`
- `vault/certs/*`
- `waf/certs/*`

Keep them local.
