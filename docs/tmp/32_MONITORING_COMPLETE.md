# 📊 Complete Monitoring System & Observability

**Last Updated:** December 13, 2025

Consolidates: `MONITORING_SYSTEM.md`, `PHASE2_MONITORING_SYSTEM.md`, `LIVE_MONITORING_GUIDE.md`, `COALITION_FETCHING_LOGS.md`

---

## 🎯 Overview

The monitoring system provides real-time visibility into:
- Pipeline execution status
- Data sync progress & metrics
- Error tracking & recovery
- Database health & integrity
- API call rate limiting
- Log file management

---

## 📈 Available Monitoring Tools

### 1. **Status Check** (Real-time)
```bash
./.cache/bin/monitoring-agent system_health
```
Shows:
- Service health (postgres, nginx)
- Last sync timestamp
- Backlog queue depth
- Row counts per table
- Disk usage

### 2. **Log Tail** (Live Stream)
```bash
tail -f logs/backlog_worker.log
# or specific metric
tail -f logs/42_token_refresh.log
```

### 3. **Database Queries** (Ad-hoc)
```bash
make db
> SELECT COUNT(*) FROM delta_campus_projects;
> SELECT id, name FROM campuses LIMIT 5;
```

### 4. **Backlog Status** (Detailed)
```bash
./.cache/bin/toolkit-agent backlog_helper status
```
Shows:
- Pending backlog user count
- Quick queue health snapshot

### 5. **Monitoring Dashboard** (Optional)
```bash
./.cache/bin/monitoring-agent system_health
```
Renders TUI with:
- Sync progress bars
- Error count trending
- API call rate gauge
- Row count changes

---

## 📍 Log Files & Locations

| Log | Path | Purpose |
|-----|------|---------|
| Token Refresh | `logs/42_token_refresh.log` | OAuth token operations |
| Backlog Worker | `logs/backlog_worker.log` | Phase 09 polling |
| Stable Tables | `logs/nightly_stable_tables.log` | Metadata sync (01-08) |
| Init DB | `logs/init_db_*.log` | Schema creation |
| Cron Output | `logs/cron_output.log` | Scheduled task output |

**Log Rotation**: Configured via `/etc/logrotate.d/42network`
- Retention: 30 days
- Max size: 100MB per file
- Compression: gzip

---

## 🔍 What to Monitor in Real-Time

### Token Operations (42_token_refresh.log)
```
[12:34:56] Token TTL: 5400s (>3600s) → No refresh needed ✅
[12:35:12] Token TTL: 1200s (<3600s) → Refreshing... ✅
[12:35:45] ✅ Token refreshed, new TTL: 7200s
```
**Look for**: Any `❌` or 401 errors → Token issue

### Backlog Processing (backlog_worker.log)
```
[12:00:00] Starting backlog sync, queue depth: 1,234
[12:00:15] Fetched batch 1 (10 items), API calls: 1/120
[12:00:30] Loaded batch 1 → 10 rows inserted
[12:01:00] Backlog complete, total: 1,234 items in 60s
```
**Look for**: Rate limiting errors (429) or FK violations

### Stable Table Sync (nightly_stable_tables.log)
```
[00:00:00] Phase 1: Fetching cursus... 1 row
[00:00:10] Phase 2: Fetching campus... 54 rows
...
[00:04:00] ✅ All 8 phases complete, 36,254 total rows
```
**Look for**: Empty responses or orphaned FK errors

---

## ⚠️ Common Issues & Recovery

### Issue: Token Expired (401 errors)
**Symptom**: `{"error": "401 Unauthorized"}`
**Root cause**: Token TTL expired
**Recovery**: 
```bash
./.cache/bin/token-manager-agent refresh
# or let next script run (has auto-recovery)
```

### Issue: Rate Limit Exceeded (429 errors)
**Symptom**: `{"error": "429 Too Many Requests"}`
**Root cause**: >120 API calls/hour
**Recovery**: 
```bash
# Reduce fetch batch size in backlog_config.sh
BACKLOG_FETCH_BATCH_SIZE=5  # (was 10)
# Then retry
```

### Issue: Foreign Key Violation
**Symptom**: `ERROR: insert or update violates foreign key`
**Root cause**: Loading in wrong order (e.g., campus_projects before campuses)
**Recovery**: See CURSUS_21_DATA_PIPELINE.md "Critical Ordering"

### Issue: Orphaned Rows
**Symptom**: Some campus_id values don't match any campuses
**Root cause**: API returned stale data
**Recovery**: Automatic (CASCADE DELETE cleans up)

---

## 📊 Metrics & KPIs

### Sync Performance
- **Target**: Full sync <4 minutes
- **Current**: 3m 36s (fresh), 30s (cached)
- **Calculation**: API calls × 1.1s avg per call

### API Utilization
- **Limit**: 120 calls/hour
- **Current**: 77 calls/hour (64%)
- **Safe margin**: 43 calls for growth

### Data Quality
- **FK violations**: 0 (verified ✅)
- **NULL primary keys**: 0 (verified ✅)
- **Duplicate rows**: 0 (upserts on PK)
- **Orphaned rows**: 0 (automatic cleanup)

### Error Recovery
- **Token refresh**: Automatic on <1h TTL
- **API retry**: 3 attempts with backoff
- **Database**: Transactional COPY (all-or-nothing)

---

## 🔐 Monitoring Best Practices

✅ **Do**: Check logs daily for unexpected patterns
✅ **Do**: Monitor API call count trending
✅ **Do**: Verify cron execution timestamps in logs
✅ **Do**: Confirm log rotation working (check /var/log)

❌ **Don't**: Ignore repeated 401 errors
❌ **Don't**: Let logs grow unbounded (setup rotation)
❌ **Don't**: Query delta_* tables (they're temporary)
❌ **Don't**: Stop monitoring during major deployments

---

## 🚨 Alert Thresholds

Set up monitoring alerts for:
| Metric | Threshold | Action |
|--------|-----------|--------|
| API calls/min | >2 | Check for loops |
| 401 errors | Any | Refresh token |
| 429 errors | Any | Reduce batch size |
| DB size | >10GB | Archive old data |
| Log files | >1GB total | Rotate manually |
| Sync time | >10min | Investigate API |

---

## 🔧 Monitoring Configuration

**Enable metrics in backlog_config.sh**:
```bash
BACKLOG_ENABLE_METRICS=true
BACKLOG_METRIC_INTERVAL=300  # 5 minutes
```

**Check current metrics**:
```bash
cat logs/metrics_latest.json | jq '.'
```

**Export metrics for external tools**:
```bash
./.cache/bin/monitoring-agent events_diff
# Outputs Prometheus-compatible format
```

---

## 📁 Related Documentation

- **Backlog System**: `01_BACKLOG_CONSOLIDATED.md`
- **Configuration**: `00_CONFIGURATION_CONSOLIDATED.md`
- **Pipeline Architecture**: `CURSUS_21_DATA_PIPELINE.md`
- **Commands Reference**: `COMMAND_REFERENCE.md`
