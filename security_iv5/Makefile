SHELL := /bin/bash
.DEFAULT_GOAL := help

help:
	@echo "IV.5 Cybersecurity"
	@echo ""
	@echo "  make bootstrap   Generate certs, init Vault, seed secrets, render env"
	@echo "  make certs       Generate local CA + Vault/WAF TLS certificates"
	@echo "  make vault-init  Start, initialize and unseal Vault"
	@echo "  make seed        Import current ../.env and ../transcendance.config into Vault"
	@echo "  make render      Render runtime/rendered/transcendence.vault.env from Vault"
	@echo "  make waf-up      Start the hardened ModSecurity/CRS WAF"
	@echo "  make waf-down    Stop the WAF"
	@echo "  make vault-logs  Show Vault logs"
	@echo "  make waf-logs    Show WAF logs"
	@echo "  make status      Show both compose projects"
	@echo "  make test        Run WAF smoke checks"

bootstrap: certs vault-init seed render

certs:
	@./bin/generate-certs.sh

vault-init: certs
	@./bin/vault-init.sh

seed:
	@./bin/seed-from-current-config.sh

render:
	@./bin/render-compose-env.sh

waf-up: certs
	@./bin/waf-up.sh

waf-down:
	@./bin/waf-down.sh

vault-logs:
	@./bin/common.sh vault-logs

waf-logs:
	@./bin/common.sh waf-logs

status:
	@./bin/common.sh status

test:
	@./bin/test-waf.sh
