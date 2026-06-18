# ============================================================================ #
#  Transcendence Deployment Makefile
#  Purpose: Single-command deployment to production (<1 hour)
# ============================================================================ #

CONFIG_FILE := ../transcendance.config

-include ../.env
-include $(CONFIG_FILE)

SHELL := /bin/bash
.DEFAULT_GOAL := deploy

RUNTIME_DIR := ../runtime
RUNTIME_BIN_DIR := $(RUNTIME_DIR)/cache/bin

ORCHESTRATION_AGENT := $(RUNTIME_BIN_DIR)/orchestration-agent
OPS_AGENT := $(RUNTIME_BIN_DIR)/ops-agent
TOKEN_MANAGER_AGENT := $(RUNTIME_BIN_DIR)/token-manager-agent
API42_CLIENT_AGENT := $(RUNTIME_BIN_DIR)/api42-client-agent

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

CONFIGURED_DOCKER_GID := $(strip $(DOCKER_GID))
HOST_UID ?= $(shell id -u)
HOST_GID ?= $(shell id -g)
DOCKER_SOCKET_GID := $(strip $(shell stat -c %g /var/run/docker.sock 2>/dev/null || true))
DOCKER_GROUP_GID := $(strip $(shell getent group docker 2>/dev/null | cut -d: -f3))
DOCKER_GID := $(or $(DOCKER_SOCKET_GID),$(CONFIGURED_DOCKER_GID),$(DOCKER_GROUP_GID),100)

ABS_REPO_ROOT := $(abspath .)
ABS_RUNTIME_DIR := $(abspath $(RUNTIME_DIR))
ABS_ENV_ROOT := $(abspath ..)
ABS_DATA_ROOT := $(abspath ../data)
ABS_ORCHESTRATION_AGENT := $(abspath $(ORCHESTRATION_AGENT))
ABS_OPS_AGENT := $(abspath $(OPS_AGENT))
ABS_TOKEN_MANAGER_AGENT := $(abspath $(TOKEN_MANAGER_AGENT))
ABS_API42_CLIENT_AGENT := $(abspath $(API42_CLIENT_AGENT))
SCHEMA_TABLES := $(strip $(shell awk '/^CREATE TABLE IF NOT EXISTS / {print $$6}' sql/schema.sql 2>/dev/null))
SCHEMA_TABLE_COUNT := $(words $(SCHEMA_TABLES))

COMPOSE_ENV = PROJECT_ROOT=$(ABS_REPO_ROOT) \
	RUNTIME_ROOT=$(ABS_RUNTIME_DIR) \
	ENV_ROOT=$(ABS_ENV_ROOT) \
	DATA_ROOT=$(ABS_DATA_ROOT) \
	DB_DATA_DIR=$(DB_DATA_DIR) \
	DB_VOLUME=$(DB_VOLUME) \
	DB_HOST=$(DB_HOST) \
	DB_PORT=$(DB_PORT) \
	DB_NAME=$(DB_NAME) \
	DB_USER=$(DB_USER) \
	DB_PASSWORD=$(DB_PASSWORD) \
	USER_DATA_STALE_AFTER_S=$(USER_DATA_STALE_AFTER_S) \
	AUTO_MAINTENANCE_INTERVAL_S=$(AUTO_MAINTENANCE_INTERVAL_S) \
	ADMIN_API42_HEALTH_CACHE_S=$(ADMIN_API42_HEALTH_CACHE_S) \
	CAMPUS_ID=$(CAMPUS_ID) \
	WEB_PORT=$(WEB_PORT) \
	HOST_UID=$(HOST_UID) \
	HOST_GID=$(HOST_GID) \
	DOCKER_GID=$(DOCKER_GID)
COMPOSE = $(COMPOSE_ENV) docker compose -f infra/docker-compose.yml

ORCHESTRA_ENV := ROOT_DIR=$(ABS_REPO_ROOT) \
	RUNTIME_DIR=$(ABS_RUNTIME_DIR) \
	ORCHESTRATION_BINARY=$(ABS_ORCHESTRATION_AGENT) \
	OPS_BINARY=$(ABS_OPS_AGENT) \
	TOKEN_MANAGER_BINARY=$(ABS_TOKEN_MANAGER_AGENT) \
	API42_CLIENT_BINARY=$(ABS_API42_CLIENT_AGENT) \
	API_CLIENT_BINARY=$(ABS_API42_CLIENT_AGENT) \
	DB_HOST=$(DB_HOST) \
	DB_PORT=$(DB_PORT) \
	DB_NAME=$(DB_NAME) \
	DB_USER=$(DB_USER) \
	DB_PASSWORD=$(DB_PASSWORD) \
	WEB_PORT=$(WEB_PORT) \
	CAMPUS_ID=$(CAMPUS_ID)
PREPARE_RUNTIME = REPO_ROOT="$(ABS_REPO_ROOT)" \
	ROOT_DIR="$(ABS_REPO_ROOT)" \
	bash "$(ABS_REPO_ROOT)/scripts/prepare_runtime.sh"
VALIDATE_CONFIG = CONFIG_FILE="$(CONFIG_FILE)" \
	REPO_ROOT="$(ABS_REPO_ROOT)" \
	CLI_CAMPUS_ID="$(CLI_CAMPUS_ID)" \
	bash "$(ABS_REPO_ROOT)/scripts/validate_config.sh"
VALIDATE_ENV = ENV_FILE="$(ABS_ENV_ROOT)/.env" \
	bash "$(ABS_REPO_ROOT)/scripts/validate_env.sh"
ENSURE_DB_PERMISSIONS = DB_DATA_DIR="$(DB_DATA_DIR)" \
	DB_CONTAINER_UID="$(DB_CONTAINER_UID)" \
	DB_CONTAINER_GID="$(DB_CONTAINER_GID)" \
	bash "$(ABS_REPO_ROOT)/scripts/ensure_db_permissions.sh"
RELEASE_TAG = ROOT_DIR="$(ABS_REPO_ROOT)" \
	RUNTIME_DIR="$(ABS_RUNTIME_DIR)" \
	bash "$(ABS_REPO_ROOT)/scripts/release_tag.sh"
ROLLBACK = ROOT_DIR="$(ABS_REPO_ROOT)" \
	RUNTIME_DIR="$(ABS_RUNTIME_DIR)" \
	TAG="$(TAG)" \
	bash "$(ABS_REPO_ROOT)/scripts/rollback.sh"

# Default values (can be overridden)
DB_HOST ?= localhost
DB_USER ?= api42
DB_PASSWORD ?= api42
DB_NAME ?= api42
DB_PORT ?= 5432
DB_VOLUME ?= transcendence_db_data
DB_DATA_DIR ?= $(ABS_DATA_ROOT)/postgres
DB_DATA_DIR := $(abspath $(DB_DATA_DIR))
DB_CONTAINER_UID ?= 999
DB_CONTAINER_GID ?= 999
WEB_PORT ?= 8050
CAMPUS_ID ?= 21
POLL_INTERVAL ?= 60000
AUTO_MAINTENANCE_INTERVAL_S ?= 3600
AUTO_MAINTENANCE_REMOVE_ORPHANS ?= 0
USER_DATA_STALE_AFTER_S ?= 600
ADMIN_API42_HEALTH_CACHE_S ?= 60
HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX ?= 200
HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX ?= 1000
HEALTH_DEGRADED_PROCESS_QUEUE_MAX ?= 200
CORE_SERVICES := db api web
MONITORING_SERVICES := prometheus node-exporter grafana cadvisor
AUTO_SERVICES := db api web detector fetcher_internal fetcher-external-1 fetcher-external-2 fetcher-external-3 upserter-users upserter-events $(MONITORING_SERVICES)
MANUAL_SERVICES := detector
ALL_TOOLING := $(ORCHESTRATION_AGENT) $(OPS_AGENT) $(TOKEN_MANAGER_AGENT) $(API42_CLIENT_AGENT)

# CAMPUS_ID is sourced from ../transcendance.config only (no CLI override)
CFG_CAMPUS_ID := $(strip $(shell awk -F= '/^[[:space:]]*CAMPUS_ID[[:space:]]*=/{v=$$2} END{gsub(/^[[:space:]]+|[[:space:]]+$$/,"",v); gsub(/^"|"$$/,"",v); sub(/[[:space:]]*#.*/,"",v); print v}' $(CONFIG_FILE) 2>/dev/null))
CLI_CAMPUS_ID := $(if $(filter command line,$(origin CAMPUS_ID)),$(CAMPUS_ID),)
override CAMPUS_ID := $(CFG_CAMPUS_ID)
DETECTOR_INTERVAL_CFG := $(strip $(shell awk -F: '/^[[:space:]]+DETECTOR_INTERVAL[[:space:]]*:/{v=$$2; gsub(/^[[:space:]]+|[[:space:]]+$$/,"",v); print v; exit}' infra/docker-compose.yml 2>/dev/null))
TIME_WINDOW_CFG := $(strip $(shell awk -F: '/^[[:space:]]+TIME_WINDOW[[:space:]]*:/{v=$$2; gsub(/^[[:space:]]+|[[:space:]]+$$/,"",v); print v; exit}' infra/docker-compose.yml 2>/dev/null))

# ============================================================================ #
#  HELP
# ============================================================================ #

help:
	@echo "╔════════════════════════════════════════════════════════════════╗"
	@echo "║          Transcendence Deployment (< 1 hour setup)            ║"
	@echo "╚════════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "MAIN COMMANDS:"
	@echo "  make check               → Verify environment before deploy"
	@echo "  make deploy              → Deploy using CAMPUS_ID from ../transcendance.config (optional: CODE=<oauth_code> for bootstrap)"
	@echo "  make status              → Check running services"
	@echo "  make logs                → Show recent logs and return"
	@echo "  make health              → Refresh admin system-health snapshot only"
	@echo "  make maintenance         → One-shot: token refresh + backup + health snapshot (+ archive)"
	@echo "  make maintenance-auto-start  → Start/reconcile containerized auto-maintenance scheduler"
	@echo "  make maintenance-auto-stop   → Stop containerized auto-maintenance scheduler"
	@echo "  make maintenance-auto-status → Show scheduler container status + latest state"
	@echo "  make maintenance-auto-once   → Run one maintenance+cleanup cycle now"
	@echo "  make cleanup             → Trim logs only (queues untouched)"
	@echo ""
	@echo "RELEASES / ROLLBACK:"
	@echo "  make release             → Tag current images with the git SHA (run automatically by deploy)"
	@echo "  make rollback TAG=<sha>  → Restart services on a previously released image tag (no rebuild)"
	@echo ""
	@echo "PORT CONFIGURATION:"
	@echo "  set WEB_PORT in ../transcendance.config → Change public web port"
	@echo ""
	@echo "UTILITIES:"
	@echo "  make up                  → Start all services (including detector)"
	@echo "  make up-detector         → Start detector only"
	@echo "  make stop-detector       → Stop detector"
	@echo "  make down                → Stop all services"
	@echo "  make clean               → Stop + remove images"
	@echo "  make fclean              → Wipe runtime folder (keeps DB data)"
	@echo "  make re                  → Full reset then redeploy (fclean + deploy)"
	@echo "  make db                  → Open PostgreSQL shell"
	@echo "  make exchange CODE=...   → Exchange OAuth authorization code into repo/.oauth_state"
	@echo ""

# ============================================================================ #
#  DEPLOYMENT (1-HOUR TARGET)
# ============================================================================ #

$(ORCHESTRATION_AGENT): tools/orchestra/src/orchestration.cpp \
	tools/orchestra/src/internal/orchestra_internal.hpp \
	tools/orchestra/src/internal/core.cpp \
	tools/orchestra/src/internal/fetch.cpp \
	tools/orchestra/src/internal/load.cpp \
	tools/orchestra/src/internal/flow.cpp \
	$(RUNTIME_BIN_DIR)
	@$(CXX) $(CXXFLAGS) \
		tools/orchestra/src/orchestration.cpp \
		tools/orchestra/src/internal/core.cpp \
		tools/orchestra/src/internal/fetch.cpp \
		tools/orchestra/src/internal/load.cpp \
		tools/orchestra/src/internal/flow.cpp \
		-o $@

$(OPS_AGENT): tools/ops/src/ops.cpp \
	tools/ops/src/internal/ops_internal.hpp \
	tools/ops/src/internal/runtime.cpp \
	tools/ops/src/internal/service_health.cpp \
	tools/ops/src/internal/system_health.cpp \
	tools/ops/src/internal/events.cpp \
	tools/ops/src/internal/backup.cpp \
	tools/ops/src/internal/maintenance.cpp \
	$(RUNTIME_BIN_DIR)
	@$(CXX) $(CXXFLAGS) \
		tools/ops/src/ops.cpp \
		tools/ops/src/internal/runtime.cpp \
		tools/ops/src/internal/service_health.cpp \
		tools/ops/src/internal/system_health.cpp \
		tools/ops/src/internal/events.cpp \
		tools/ops/src/internal/backup.cpp \
		tools/ops/src/internal/maintenance.cpp \
		-o $@

$(TOKEN_MANAGER_AGENT): tools/token-manager/src/token_manager.cpp $(RUNTIME_BIN_DIR)
	@$(CXX) $(CXXFLAGS) $< -o $@

$(API42_CLIENT_AGENT): tools/api42-client/src/api_client.cpp $(RUNTIME_BIN_DIR)
	@$(CXX) $(CXXFLAGS) $< -o $@

$(RUNTIME_BIN_DIR):
	@mkdir -p "$@"

check: $(ORCHESTRATION_AGENT) $(TOKEN_MANAGER_AGENT) $(API42_CLIENT_AGENT)
	@$(VALIDATE_CONFIG)
	@$(VALIDATE_ENV)
	@$(PREPARE_RUNTIME)
	@$(ORCHESTRA_ENV) $(ORCHESTRATION_AGENT) check_environment

exchange:
	@if [ -z "$(CODE)" ]; then \
		echo "❌ Missing CODE."; \
		echo "   Usage: make exchange CODE=<authorization_code>"; \
		exit 1; \
	fi
	@tm_bin="$(ABS_TOKEN_MANAGER_AGENT)"; \
	tmp_dir=""; \
	if [ ! -x "$$tm_bin" ]; then \
		echo "ℹ️  token-manager-agent not found in runtime cache, building temporary binary..."; \
		tmp_dir="$$(mktemp -d)"; \
		tm_bin="$$tmp_dir/token-manager-agent"; \
		$(CXX) $(CXXFLAGS) tools/token-manager/src/token_manager.cpp -o "$$tm_bin"; \
	fi; \
	ROOT_DIR="$(ABS_REPO_ROOT)" "$$tm_bin" exchange "$(CODE)"; \
	rc="$$?"; \
	if [ -n "$$tmp_dir" ]; then rm -rf "$$tmp_dir"; fi; \
	exit "$$rc"

tooling: $(ALL_TOOLING)

deploy:
	@if [ -n "$(CODE)" ]; then \
		echo "🔐 Step 1/10: OAuth exchange (CODE provided)"; \
		$(MAKE) exchange CODE="$(CODE)"; \
	else \
		echo "⏭️  Step 1/10: OAuth exchange skipped (no CODE)"; \
	fi
	@echo "🔎 Step 2/10: validate transcendance config..."
	@$(VALIDATE_CONFIG)
	@echo "🗂️  Step 3/10: prepare runtime folders..."
	@$(PREPARE_RUNTIME)
	@echo "🗄️  Step 4/10: start DB + create/check database..."
	@$(MAKE) up-db
	@echo "🛠️  Step 5/10: build local agents..."
	@$(MAKE) tooling
	@echo "⚛️  Step 6/10: build React frontend..."
	@npm --prefix "$(ABS_REPO_ROOT)/frontend" install
	@npm --prefix "$(ABS_REPO_ROOT)/frontend" run build
	@echo "🐳 Step 7/10: build + start core services (detector included, maintenance deferred)..."
	@$(MAKE) up-services-core
	@$(MAKE) release
	@echo "🚀 Starting Transcendence deployment..."
	@echo "   Campus ID: $(CAMPUS_ID)"
	@echo "   Target: <1 hour complete setup"
	@echo ""
	@echo "🎼 Step 8/10: run orchestration flow (fetch/load metadata)..."
	@$(ORCHESTRA_ENV) $(ORCHESTRATION_AGENT) orchestra
	@echo "🔁 Step 9/10: start maintenance scheduler (first cycle includes backup + health)..."
	@$(MAKE) maintenance-auto-start
	@echo "📊 Step 10/10: write system health snapshot..."
	@$(ORCHESTRA_ENV) $(OPS_AGENT) system_health
	@echo ""
	@echo "✅ Deployment complete!"
	@echo "   Web: http://localhost:$(WEB_PORT)"
	@echo "   Campus: $(CAMPUS_ID)"
	@echo "   Detector: started automatically"
	@echo "   Data: live refresh active"
	@echo "   Config:"
	@echo "     CAMPUS_ID=$(CAMPUS_ID)"
	@echo "     DB_PORT=$(DB_PORT)"
	@echo "     DB_DATA_DIR=$(DB_DATA_DIR)"
	@echo "     WEB_PORT=$(WEB_PORT)"
	@echo "     DETECTOR_INTERVAL=$(DETECTOR_INTERVAL_CFG)s"
	@echo "     TIME_WINDOW=$(TIME_WINDOW_CFG)s"
	@echo ""

# ============================================================================ #
#  SERVICE STARTUP
# ============================================================================ #

up:
	@$(MAKE) up-db
	@$(MAKE) up-services

up-db:
	@if [ -n "$(CONFIGURED_DOCKER_GID)" ] && [ "$(CONFIGURED_DOCKER_GID)" != "$(DOCKER_GID)" ]; then \
		echo "ℹ️  /var/run/docker.sock gid=$(DOCKER_GID); ignoring configured DOCKER_GID=$(CONFIGURED_DOCKER_GID)"; \
	fi
	@echo "🔐 Preparing PostgreSQL data directory ($(DB_DATA_DIR))..."
	@$(ENSURE_DB_PERMISSIONS)
	@echo "📦 Starting database service..."
	$(COMPOSE) up -d db
	@echo "   DB endpoint: $(DB_HOST):$(DB_PORT) (user=$(DB_USER), db=$(DB_NAME))"
	@echo "⏳ Waiting for database bootstrap ($(DB_NAME))..."
	@db_create_attempted=0; \
	for i in $$(seq 1 90); do \
		if $(COMPOSE) exec -T db sh -lc "pg_isready -U $(DB_USER) -d postgres >/dev/null 2>&1"; then \
			if $(COMPOSE) exec -T db sh -lc "psql -U $(DB_USER) -d postgres -Atc \"SELECT 1 FROM pg_database WHERE datname='$(DB_NAME)'\" | grep -q 1"; then \
				echo "✅ Database ready"; \
				break; \
			fi; \
			if [ "$$db_create_attempted" -eq 0 ]; then \
				echo "ℹ️  Database '$(DB_NAME)' not found yet; trying to create it..."; \
				if $(COMPOSE) exec -T db sh -lc "psql -U $(DB_USER) -d postgres -v ON_ERROR_STOP=1 -c \"CREATE DATABASE \\\"$(DB_NAME)\\\";\"" >/dev/null 2>&1; then \
					echo "✅ Created database '$(DB_NAME)'"; \
				else \
					echo "⚠️  Automatic CREATE DATABASE failed (will keep waiting)"; \
				fi; \
				db_create_attempted=1; \
			fi; \
		fi; \
		if [ "$$i" -eq 90 ]; then \
			echo "❌ Database bootstrap timed out"; \
			echo "🔎 DB diagnostics (status + recent logs + visible databases):"; \
			$(COMPOSE) ps db || true; \
			$(COMPOSE) logs --tail=120 db || true; \
			$(COMPOSE) exec -T db sh -lc "psql -U $(DB_USER) -d postgres -Atc \"SELECT datname FROM pg_database ORDER BY 1\"" || true; \
			exit 1; \
		fi; \
		sleep 1; \
	done
	@if $(COMPOSE) exec -T db sh -lc "psql -U $(DB_USER) -d $(DB_NAME) -Atc 'SELECT 1' | grep -q '^1$$'"; then \
		echo "✅ Database connection test passed ($(DB_NAME))"; \
	else \
		echo "❌ Database connection test failed ($(DB_NAME))"; \
		exit 1; \
	fi
	@echo "🧱 Applying database schema..."
	@$(COMPOSE) exec -T db psql -U $(DB_USER) -d $(DB_NAME) -v ON_ERROR_STOP=1 < sql/schema.sql
	@missing_tables=""; \
	for table in $(SCHEMA_TABLES); do \
		if ! $(COMPOSE) exec -T db sh -lc "psql -U $(DB_USER) -d $(DB_NAME) -Atc \"SELECT to_regclass('public.$$table')\"" | grep -q "^$$table$$"; then \
			missing_tables="$$missing_tables $$table"; \
		fi; \
	done; \
	if [ -z "$$missing_tables" ]; then \
		echo "✅ Database schema ready ($(SCHEMA_TABLE_COUNT) table(s) verified from sql/schema.sql)"; \
	else \
		echo "❌ Database schema verification failed"; \
		echo "   Missing table(s):$$missing_tables"; \
		exit 1; \
	fi

up-services-core:
	@echo "📦 Building and starting Docker services ($(filter-out db,$(AUTO_SERVICES)))..."
	$(COMPOSE) up -d --build $(filter-out db,$(AUTO_SERVICES))
	@echo "✅ Services started"
	@$(MAKE) status

up-services: up-services-core
	@$(MAKE) maintenance-auto-start

# ============================================================================ #
#  RELEASES / ROLLBACK
# ============================================================================ #

release:
	@echo "🏷️  Tagging current images with the current git SHA for rollback..."
	@$(RELEASE_TAG)

rollback:
	@if [ -z "$(TAG)" ]; then \
		echo "❌ Usage: make rollback TAG=<git-sha>"; \
		echo "   See $(ABS_RUNTIME_DIR)/logs/state/releases.log for available tags"; \
		exit 1; \
	fi
	@$(ROLLBACK)
	@$(MAKE) status

up-detector:
	@echo "📦 Starting detector ($(MANUAL_SERVICES))..."
	$(COMPOSE) up -d $(MANUAL_SERVICES)
	@echo "✅ Detector started"
	@echo "♻️  Refreshing admin health snapshot..."
	@$(MAKE) health >/dev/null
	@$(MAKE) status

stop-detector:
	@echo "⏸️  Stopping detector..."
	$(COMPOSE) stop $(MANUAL_SERVICES)
	@echo "✅ Detector stopped"
	@echo "♻️  Refreshing admin health snapshot..."
	@$(MAKE) health >/dev/null
	@$(MAKE) status

# ============================================================================ #
#  SERVICE MANAGEMENT
# ============================================================================ #

status:
	@echo "🔍 Service status:"
	@$(COMPOSE) ps
	@echo ""

stop:
	@echo "⏸️  Stopping services..."
	$(COMPOSE) stop
	@echo "✅ Services stopped"

down:
	@echo "🛑 Shutting down services..."
	$(COMPOSE) down --remove-orphans
	@echo "✅ Services removed"

logs:
	$(COMPOSE) logs --tail=200

health: $(OPS_AGENT)
	@$(PREPARE_RUNTIME)
	@ROOT_DIR="$(ABS_REPO_ROOT)" \
	 RUNTIME_DIR="$(ABS_RUNTIME_DIR)" \
	 HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX="$(HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX)" \
	 HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX="$(HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX)" \
	 HEALTH_DEGRADED_PROCESS_QUEUE_MAX="$(HEALTH_DEGRADED_PROCESS_QUEUE_MAX)" \
	 $(OPS_AGENT) system_health

maintenance: $(OPS_AGENT) $(TOKEN_MANAGER_AGENT)
	@$(PREPARE_RUNTIME)
	@ROOT_DIR="$(ABS_REPO_ROOT)" \
	 RUNTIME_DIR="$(ABS_RUNTIME_DIR)" \
	 PROJECT_ROOT="$(ABS_REPO_ROOT)" \
	 RUNTIME_ROOT="$(ABS_RUNTIME_DIR)" \
	 ENV_ROOT="$(ABS_ENV_ROOT)" \
	 DATA_ROOT="$(ABS_DATA_ROOT)" \
	 DB_DATA_DIR="$(DB_DATA_DIR)" \
	 HOST_UID="$(HOST_UID)" \
	 HOST_GID="$(HOST_GID)" \
	 DOCKER_GID="$(DOCKER_GID)" \
	 TOKEN_MANAGER_BINARY="$(ABS_TOKEN_MANAGER_AGENT)" \
	 HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX="$(HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX)" \
	 HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX="$(HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX)" \
	 HEALTH_DEGRADED_PROCESS_QUEUE_MAX="$(HEALTH_DEGRADED_PROCESS_QUEUE_MAX)" \
	 $(OPS_AGENT) maintenance

maintenance-auto-start:
	@$(PREPARE_RUNTIME)
	@echo "🔁 Starting/reconciling maintenance-scheduler container..."
	@$(COMPOSE) up -d maintenance-scheduler
	@$(MAKE) maintenance-auto-status

maintenance-auto-stop:
	@echo "⏸️  Stopping maintenance-scheduler container..."
	@$(COMPOSE) stop maintenance-scheduler
	@$(MAKE) maintenance-auto-status

maintenance-auto-status:
	@echo "🔍 Scheduler container status:"
	@$(COMPOSE) ps maintenance-scheduler
	@echo ""
	@if [ -f "$(ABS_RUNTIME_DIR)/logs/state/scheduler_state.json" ]; then \
		echo "state_file=$(ABS_RUNTIME_DIR)/logs/state/scheduler_state.json"; \
		tail -n 20 "$(ABS_RUNTIME_DIR)/logs/state/scheduler_state.json"; \
	else \
		echo "state_file=missing ($(ABS_RUNTIME_DIR)/logs/state/scheduler_state.json)"; \
	fi

maintenance-auto-once: $(OPS_AGENT) $(TOKEN_MANAGER_AGENT)
	@$(PREPARE_RUNTIME)
	@echo "🛠️  Running one maintenance cycle (maintenance + cleanup)..."
	@ROOT_DIR="$(ABS_REPO_ROOT)" \
	 RUNTIME_DIR="$(ABS_RUNTIME_DIR)" \
	 PROJECT_ROOT="$(ABS_REPO_ROOT)" \
	 RUNTIME_ROOT="$(ABS_RUNTIME_DIR)" \
	 ENV_ROOT="$(ABS_ENV_ROOT)" \
	 DATA_ROOT="$(ABS_DATA_ROOT)" \
	 DB_DATA_DIR="$(DB_DATA_DIR)" \
	 HOST_UID="$(HOST_UID)" \
	 HOST_GID="$(HOST_GID)" \
	 DOCKER_GID="$(DOCKER_GID)" \
	 TOKEN_MANAGER_BINARY="$(ABS_TOKEN_MANAGER_AGENT)" \
	 HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX="$(HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX)" \
	 HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX="$(HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX)" \
	 HEALTH_DEGRADED_PROCESS_QUEUE_MAX="$(HEALTH_DEGRADED_PROCESS_QUEUE_MAX)" \
	 $(OPS_AGENT) maintenance
	@ROOT_DIR="$(ABS_REPO_ROOT)" \
	 RUNTIME_DIR="$(ABS_RUNTIME_DIR)" \
	 $(OPS_AGENT) cleanup
	@if [ "$(AUTO_MAINTENANCE_REMOVE_ORPHANS)" = "1" ]; then \
		echo "♻️  AUTO_MAINTENANCE_REMOVE_ORPHANS=1 -> docker compose up -d --remove-orphans"; \
		$(COMPOSE) up -d --remove-orphans; \
	fi

cleanup: $(OPS_AGENT)
	@$(PREPARE_RUNTIME)
	@ROOT_DIR="$(ABS_REPO_ROOT)" \
	 RUNTIME_DIR="$(ABS_RUNTIME_DIR)" \
	 $(OPS_AGENT) cleanup

db:
	$(COMPOSE) exec db psql -U $(DB_USER) -d $(DB_NAME)

# ============================================================================ #
#  CLEANUP
# ============================================================================ #

clean: down
	@echo "🧹 Removing images..."
	$(COMPOSE) down --remove-orphans --rmi local
	@echo "✅ Clean complete"

fclean: clean
	@echo "🧼 Full cleanup (wiping runtime folder, preserving PostgreSQL data)..."
	@if [ -z "$(DB_DATA_DIR)" ] || [ "$(DB_DATA_DIR)" = "/" ]; then \
		echo "❌ Refusing fclean: invalid DB_DATA_DIR='$(DB_DATA_DIR)'"; \
		exit 1; \
	fi
	@echo "🔒 Protected DB data dir: $(DB_DATA_DIR)"
	@$(COMPOSE) down --remove-orphans --rmi all
	@echo "🧹 Removing runtime directory: $(ABS_RUNTIME_DIR)"
	@rm -rf "$(ABS_RUNTIME_DIR)" 2>/dev/null || true
	@if [ -d "$(ABS_RUNTIME_DIR)" ]; then \
		echo "   → Ownership prevents direct delete; retrying via Docker helper..."; \
		runtime_parent=$$(dirname "$(ABS_RUNTIME_DIR)"); \
		runtime_base=$$(basename "$(ABS_RUNTIME_DIR)"); \
		docker run --rm -v "$$runtime_parent":/data alpine sh -c "rm -rf /data/$$runtime_base" >/dev/null 2>&1 || true; \
	fi
	@if [ -d "$(ABS_RUNTIME_DIR)" ]; then \
		echo "❌ Could not remove $(ABS_RUNTIME_DIR)"; \
		echo "   Run once with sudo: sudo rm -rf $(ABS_RUNTIME_DIR)"; \
		exit 1; \
	fi
	@echo "🧹 Removing repo build cache (.cache)..."; \
	[[ -d .cache ]] && rm -rf .cache || true; \
	echo "✅ Full cleanup complete"; \
	echo "   Runtime removed: $(ABS_RUNTIME_DIR)"; \
	echo "   PostgreSQL data preserved at: $(DB_DATA_DIR)"; \
	echo "   (fclean never removes data/)"

re: fclean deploy

# ============================================================================ #
#  PHONY TARGETS
# ============================================================================ #

.PHONY: help deploy check exchange \
	up up-db up-services-core up-services up-detector stop-detector stop down status logs health maintenance \
	maintenance-auto-start maintenance-auto-stop maintenance-auto-status maintenance-auto-once cleanup db \
	release rollback \
	clean fclean re
