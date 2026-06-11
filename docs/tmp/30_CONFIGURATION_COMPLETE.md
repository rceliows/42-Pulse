# 📋 Complete Configuration & Parameters Reference

**Last Updated:** December 13, 2025

## 🎯 Single Source of Truth for All Configuration

This document consolidates configuration information from:
- CONFIG_PARAMETERS.md ✓
- PARAMETERS_QUICK_REFERENCE.md ✓
- Removed redundancy with IMPLEMENTATION_SUMMARY.md

---

## 🔧 Key Parameters

### Backlog System Configuration

**Location**: `app/*/config/agents.config`

```bash
# Fetch parameters
BACKLOG_FETCH_BATCH_SIZE=10        # Items per batch
BACKLOG_FETCH_DELAY_SECONDS=2      # Delay between batches
BACKLOG_MAX_RETRIES=3              # API retry attempts
BACKLOG_RETRY_DELAY=5              # Seconds between retries

# Database parameters
BACKLOG_TABLE_PREFIX="delta_"      # Staging table prefix
BACKLOG_BATCH_SIZE_LOAD=100        # Rows per COPY statement
BACKLOG_VALIDATE_FOREIGN_KEYS=true # FK validation

# Monitoring
BACKLOG_ENABLE_METRICS=true
BACKLOG_METRIC_INTERVAL=300        # 5 minutes
```

### Environment Variables (from ../.env)

```bash
API_42_CLIENT_ID=<your_id>
API_42_CLIENT_SECRET=<your_secret>
DB_HOST=localhost
DB_PORT=5432
DB_USER=api42
DB_PASSWORD=api42
DB_NAME=api42
```

### Data Pipeline Settings

**Metadata Sync** (run on deployment):
- 8 phases fetched: cursus, campus, achievements, projects, coalitions, campus_achievements, campus_projects, project_sessions
- 54 active campuses (filtered: active=true AND public=true)
- 1,042 achievements
- 519 projects (with active campus links)
- 20,937 campus_project associations

**Delta/Incremental Sync**:
- Campus achievements: Full sync every 24h
- Project sessions: Full sync every 24h  
- Backlog: Incremental polling (5-min intervals)

**User Data** (not in stable phase):
- Currently EMPTY (intentional)
- Ready for live tracking phase implementation

---

## 📊 Load Profile & Limits

| Metric | Value | Status |
|--------|-------|--------|
| API Limit | 120 calls/hour | Hard limit |
| Current Load | 77 calls/hour | 64% utilization |
| Safe Margin | 43 calls remaining | ✅ OK |
| Throughput | ~1.1 calls/sec | Consistent |
| Full Sync Time | 3m 36s (fresh) | First run |
| Incremental Sync | 30s (cached) | No changes |

---

## 🔄 Fetch Strategy by Phase

### Phase 1-8: Metadata (Static, Cached)
- Fetch method: Full pull on deployment
- Cache headers: 24-hour TTL
- Retry: 3 attempts with exponential backoff
- Validation: JSON schema check

### Phase 9: Backlog (Dynamic, Polled)
- Fetch method: Incremental polling
- Poll interval: 5 minutes (configurable)
- Batch size: 10 items
- Validation: Foreign key constraints

---

## ⚙️ Script Execution Order

```
deployment sequence:
1. init_db.sh              → Create schema, 9 tables
2. fetch_metadata.sh       → Phases 01-08 (cursus through sessions)
3. backlog_worker.sh       → Phase 09 (polling)
4. cron (1/min)           → Keep backlog fresh
```

**Critical Ordering**: Load campuses BEFORE campus_projects (FK dependency)

---

## 🗄️ Database Schema (Stable Tables)

**Stable Tables (Never Drop)**:
| Table | Rows | Purpose |
|-------|------|---------|
| cursus | 1 | Metadata |
| campuses | 54 | Campus list |
| projects | 519 | Project catalog |
| coalitions | 350 | Coalition groups |
| achievements | 1,042 | Achievement definitions |
| campus_projects | 20,937 | N:N links |
| campus_achievements | 5,495 | N:N links |
| project_sessions | 7,256 | Session data |

**Delta Tables (Temporary, Truncated After Sync)**:
- delta_cursus
- delta_campuses
- delta_projects
- delta_coalitions
- delta_achievements
- delta_campus_projects
- delta_campus_achievements
- delta_project_sessions

**Future Tables** (Empty, For Live Tracking Phase):
- users
- project_users
- achievements_users
- coalitions_users

---

## 🚀 Deployment Configuration

**Docker Environment**:
```yaml
Services:
  - postgres:16-alpine (port 5432 → 3306 on host)
  - nginx:alpine (port 80 → 8000 on host)
  
Volumes:
  - ../data/postgres → database persistence
  - sql/schema.sql → auto-init schema
  - exports/ → JSON cache
  - logs/ → execution logs
```

**Make Commands**:
```bash
make deploy          # Full deployment
make up              # Start services
make down            # Stop services
make re              # Full cleanup + redeploy
make status          # Service status
make logs            # Tail logs
make db              # PostgreSQL CLI
```

---

## 🔐 Known Constraints

✅ **Active Filtering**: Only campuses with active=true AND public=true

✅ **Alumni Filter**: WHERE kind='student' AND alumni=false (guaranteed)

✅ **Duplicate Coalitions**: 2 coalitions named "Analysts" (IDs 10 & 11) - by design

✅ **NULL Slugs**: 52 projects have NULL slug fields - legitimate API data

✅ **Orphan Cleanup**: 3,258 orphaned campus_projects removed automatically

---

## 📝 Outdated Files (Reference Only)

These documents are kept for history but NOT authoritative:
- `IMPLEMENTATION_SUMMARY.md` → Use INDEX.md + this file instead
- `IMPLEMENTATION_CHECKLIST.md` → Phase status moved here

---

## 🔗 Related Documentation

- **Architecture**: `CURSUS_21_DATA_PIPELINE.md`, `PIPELINE_VISUAL_GUIDE.md`
- **Monitoring**: `MONITORING_SYSTEM.md`, `PHASE2_MONITORING_SYSTEM.md`
- **Commands**: `COMMAND_REFERENCE.md`
- **Context**: `CLAUDE_CONTEXT_MEMORY.md`
