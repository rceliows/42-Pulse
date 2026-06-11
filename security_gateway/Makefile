SHELL := /bin/bash
.DEFAULT_GOAL := help

help:
	@echo "security_gateway security pack"
	@echo ""
	@echo "  make bootstrap      Generate certs, initialize Vault, seed OAuth secrets, render env"
	@echo "  make certs          Generate local CA + Vault/WAF certificates"
	@echo "  make vault-init     Start, initialize and unseal Vault"
	@echo "  make seed           Seed Vault from local/oauth.env when present"
	@echo "  make render         Render runtime/rendered/oauth-gateway.env from Vault"
	@echo "  make up             Start OAuth gateway + hardened WAF"
	@echo "  make down           Stop security_gateway services"
	@echo "  make status         Show service status"
	@echo "  make logs           Show all logs"
	@echo "  make test           Run static checks and optional live smoke checks"

bootstrap: certs vault-init seed render

certs:
	@./scripts/generate-certs.sh

vault-init: certs
	@./scripts/vault-init.sh

seed:
	@./scripts/seed-oauth.sh

render:
	@./scripts/render-env.sh

up:
	@./scripts/up.sh

down:
	@./scripts/down.sh

status:
	@./scripts/common.sh status

logs:
	@./scripts/common.sh logs

test:
	@./scripts/test.sh
