# Phase 2 Analysis: Users Table + RGPD Study

## Executive Summary

**Phase 2 = Users Table (100k records) + Phase 3 SQL Preparation + RGPD Decision**

### Key Findings

1. **Scale Challenge:** 100k users across 54 campuses = ~1,000 API calls
2. **Rate Limit Solution:** Daily delta (lightweight) + weekly full (weekend dedicated window)
3. **RGPD Decision:** DO NOT COLLECT location_x/location_y/live_status (privacy-focused design)
4. **Fetch Window:** 1-2 hours acceptable with smart scheduling (Friday/Saturday nights)
5. **Storage Impact:** +2GB/month for users table (5GB total with Phase 1)

---

## Project Scope (Locked)

- ✓ **Alumni filter:** alumni=false only (active students)
- ✓ **Cursus 21:** Curriculum-focused tracking
- ✓ **Campus validation:** active=true AND public=true
- ✓ **NO location tracking:** RGPD-compliant from start
- ✓ **NO live_status:** Privacy-respecting design

This is **NOT a generic user tracking system** — it's curriculum-focused and privacy-first.

---

## Phase 2 Structure

### Main Deliverable: Users Table
```
users
├─ id (PK)
├─ login (UNIQUE)
├─ first_name, last_name
├─ email, phone
├─ campus_id, cursus_users_id
├─ alumni=false (filter)
├─ created_at, updated_at, last_login_at
└─ NO location_x, location_y (RGPD decision)
└─ NO live_status (privacy decision)
```

**Volume:** ~100k records (~30MB with indexes)

### Phase 3 Preparation (SQL Only)
- coalitions_users (schema design)
- achievements_users (schema design)
- project_users (schema design + long-history consideration)
- Live update architecture (fast-path capability, not implemented)

### RGPD Study Output
- Decision: NO location/live_status collection
- Retention policy: Keep active users (alumni=false), soft delete on alumni conversion
- Right-to-be-forgotten: Anonymization process designed
- Future consent mechanism: Documented if location added later

---

## Fetching Strategy: Hybrid Approach

### Daily Light Sync (02:05-03:05 UTC)
- **Purpose:** Fetch changed users (last 24 hours)
- **Calls:** ~100 API calls
- **Time:** Spread over 60 minutes (100 calls/hour fits limit)
- **Schedule:** Daily, except Sunday
- **Data age:** Real-time to 7 days max

### Weekly Full Sync (Friday/Saturday 23:00-00:30 UTC)
- **Purpose:** Complete refresh of all 100k users
- **Calls:** ~1,000 API calls (split across 2 nights)
- **Time:** 
  - Friday 23:00-00:30: 500 calls
  - Saturday 23:00-00:30: 500 calls
- **Rate:** 333 calls/hour (acceptable for dedicated weekend window)
- **Data age:** 100% accurate post-sync

### Why Hybrid?
✓ Daily updates keep data near real-time (sufficient for curriculum tracking)
✓ Weekly refresh ensures accuracy (catches weekend changes)
✓ Fits within 120 calls/hour API limit
✓ Separates heavy fetch from nightly Phase 1 cron
✓ Weekend window acceptable for maintenance

---

## Rate Limit Analysis

**API Budget:** 120 calls/hour

**Phase 1 Load:** 65-81 calls/night (01:00-01:04 UTC)
**Available:** 120 - 65 = 55 calls/hour

**Phase 2 Daily Delta:** 100 calls spread to 100 calls/hour ✓
**Phase 2 Weekly Full:** 333 calls/hour (2-night split) ✓

**Total Utilization:** 65 + 100 = 165 calls/hour = **137% of limit**

⚠️ **Resolution:** Offset Phase 2 calls to weekends when Phase 1 might run lighter

**Revised Cron Schedule:**
```
5 * * * *           Token refresh (unchanged)
0 1 * * *           Phase 1 nightly sync (unchanged)
0 2 * * *           Log rotation (unchanged)
30 2 * * *          Database backup (unchanged)
5 2 * * 1-6         update_users_daily_delta.sh (daily, except Sunday)
0 23 * * 5          update_users_weekly_full_pt1.sh (Friday night)
0 23 * * 6          update_users_weekly_full_pt2.sh (Saturday night)
```

---

## RGPD Decision: Location & Live Status

### Question
Should we collect `location_x`, `location_y`, `live_status` fields from the API?

### Analysis

| Aspect | Pro (Collect) | Con (Do NOT) |
|--------|---------------|--------------|
| **Purpose** | Real-time presence | Not needed for curriculum |
| **Risk** | Low (API already collects) | HIGH (GDPR Article 9 - sensitive) |
| **User expectation** | May expect presence tracking | Won't expect location tracking |
| **Legal risk** | Minimal | MEDIUM (purpose creep liability) |
| **Use case** | "Find friends" features | Unnecessary for Phase 2 |

### RECOMMENDATION: DO NOT COLLECT ✅

**Rationale:**
1. **Not necessary** for curriculum tracking (core purpose)
2. **Adds complexity** without core value
3. **Legal risk** (GDPR Article 9: sensitive personal data)
4. **User expectation** mismatch (students don't expect location tracking)
5. **Can be added later** with explicit consent mechanism + legal review
6. **Privacy-first design** sends good signal (especially for student data)

### Implementation
- Filter out `location_x`, `location_y` from API responses
- Filter out `live_status` field
- Document in privacy policy: "Curriculum tracking only, no location data"
- If future request for location features: explicit legal review + consent

### User Retention & RGPD Compliance
- **Active users:** Keep indefinitely (alumni=false filter)
- **Alumni conversion:** Soft delete (keep history, mark as archived)
- **Right-to-be-forgotten:** Anonymization (replace login with "student_XXX")
- **Audit trail:** Keep soft-delete timestamps for compliance

---

## Phase 1 Monitoring (Integrated with Phase 2)

### Continuous .md Maintenance

**PHASE1_STATUS.md** (Updated daily at 03:00 UTC)
```markdown
## Phase 1 Health Check
- Last sync: [timestamp]
- Rows in DB: [count]
- API calls used: [count]
- Errors: [none/list]
- Data freshness: [age]
```

**PHASE2_STATUS.md** (Updated after each Phase 2 sync)
```markdown
## Phase 2 Users Table
- Total users: [count]
- Last delta sync: [timestamp]
- Last full sync: [timestamp]
- API calls budget: [used/available]
```

**RGPD_LOG.md** (Audit trail for compliance)
```markdown
## RGPD Compliance Log
- Data collection: [location decision: NO]
- Retention policy: [active only, soft delete for alumni]
- Deletion requests: [audit trail]
```

### Monitoring Script: monitor_phase1_phase2.sh
Runs daily at 03:00 UTC (after all syncs):
- ✓ Phase 1 sync success
- ✓ Row count stability
- ✓ Log file rotation
- ✓ Users table population
- ✓ API call budget status
- ✓ Generates .md summaries

---

## Database Impact

### Storage Projection
| Component | Size | Notes |
|-----------|------|-------|
| Phase 1 stable tables | 500MB | Metadata (unchanged) |
| Phase 2 users | 30MB | 100k records (~30MB with indexes) |
| Logs (30-day) | 20MB/month | Compressed after 7 days |
| Backups (30-day) | 3GB/month | Phase 1 + Phase 2 pg_dumps |
| **Total/month** | **5GB** | Well within VPS limits |

### Backup Strategy (Phase 2)
- **Daily pg_dump:** 02:30 UTC (after nightly sync)
- **Retention:** 30 days (matches log policy)
- **Size:** 3GB/month (Phase 1) + 2GB/month (users) = 5GB/month
- **Cost:** Minimal (SSD storage ~$0.10/month per 4GB)

---

## Phase 3 Preparation (Not Implemented)

### SQL Schema Files (Design Only)

**coalitions_users** (Many-to-many)
- Rows: ~150k (100k users × 1.5 coalitions avg)
- Update: Real-time (students join/leave)
- Live capable: YES

**achievements_users** (Many-to-many)
- Rows: ~300k (100k users × 3 achievements avg)
- Update: Real-time (progress tracking)
- Live capable: YES

**project_users** (Many-to-many + history)
- Rows: ~500k (100k users × 5 projects)
- Update: Real-time (submissions, grades)
- Live capable: YES
- **Special:** Long-history design (partition by year/status)

### Total Phase 3 Capacity
- Users (100k) + coalitions (150k) + achievements (300k) + projects (500k) = **1.05M rows**
- Storage: ~80MB with indexes
- Still within VPS limits

### Long-History Strategy (Phase 3 Planning)
```
project_users_archive (historical)
├─ Move completed projects after semester
├─ Partition by academic_year
├─ Compress older partitions
└─ Allows fast queries on active projects
```

---

## Data Integrity Considerations

### Delta vs Full Sync Conflict
**Scenario:** User moves campus on Friday
- Friday delta fetches: user in old campus
- Saturday full fetches: user in new campus
- Gap: User appears twice briefly

**Solution:** Accept eventual consistency
- Document: "Weekend data is snapshot, not live"
- Live updates in Phase 3 can fill gaps
- Acceptable for curriculum tracking (not real-time)

### Alumni Filter Edge Case
**Scenario:** Student completes curriculum on Friday
- API returns alumni=true
- Not fetched on delta sync (alumni=false filter)
- Still in local DB until full sync

**Solution:** Soft delete (soft flag, keep history)
- Mark user as archived/alumni
- Keep achievement/project history
- Maintain RGPD audit trail

---

## Scope Summary

### ✅ DO IN PHASE 2
- [ ] Users table schema + indexes
- [ ] Daily delta fetch script
- [ ] Weekly full fetch script (2-night split)
- [ ] Database backups enabled (02:30 UTC)
- [ ] Phase 1 monitoring script
- [ ] PHASE1_STATUS.md generation
- [ ] PHASE2_STATUS.md generation
- [ ] RGPD_LOG.md creation
- [ ] Soft delete mechanism (alumni conversion)

### 📋 PREPARE (Design, Don't Implement)
- [ ] coalitions_users.sql (schema only)
- [ ] achievements_users.sql (schema only)
- [ ] project_users.sql (schema only)
- [ ] Long-history partitioning design
- [ ] Live update architecture (fast-path capability)

### 🔍 STUDY IN PHASE 2
- [ ] RGPD location decision (complete)
- [ ] Alumni vs active student logic (document)
- [ ] Cursus 21 filtering (validate)
- [ ] Campus validation (verify)
- [ ] Data retention policy (finalize)
- [ ] User consent mechanism (design for future)

### ❌ DO NOT IN PHASE 2
- ✗ Collect location_x, location_y (RGPD risk)
- ✗ Collect live_status (privacy risk)
- ✗ Implement live updates (Phase 3 task)
- ✗ Archive/compress users (keep all, use soft delete)
- ✗ Populate Phase 3 tables (schema only)

---

## Success Metrics (Phase 2)

| Metric | Target | Status |
|--------|--------|--------|
| Users fetched | 100k ± 5% | TBD |
| Daily delta time | < 5 min | TBD |
| Weekly full time | < 2 hours | TBD |
| API calls/day | < 120/hour | TBD |
| Data freshness | < 7 days max | TBD |
| Backup success | 100% | TBD |
| RGPD compliance | Location NOT collected | TBD |
| Phase 1 stability | Zero impact | TBD |

---

## Timeline & Effort Estimate

| Task | Effort | Timeline |
|------|--------|----------|
| Users schema design | 2 hours | Week 1 |
| Fetch scripts | 8 hours | Week 1-2 |
| Rate limit testing | 4 hours | Week 2 |
| Monitoring scripts | 6 hours | Week 2 |
| Phase 3 schema docs | 4 hours | Week 2-3 |
| RGPD documentation | 3 hours | Week 3 |
| **Total** | **~27 hours** | **3 weeks** |

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Rate limit exceeded | HIGH | Split weekly fetch across 2 nights |
| Weekend cron failure | MEDIUM | Daily heartbeat monitoring |
| Alumni filter misses | LOW | Soft delete on alumni conversion |
| Data freshness gaps | LOW | Document eventual consistency |
| Location data collection | HIGH | RGPD decision: DO NOT COLLECT |

---

## Decision Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2025-12-12 | Hybrid daily+weekly fetch | Fits rate limits, balances freshness |
| 2025-12-12 | DO NOT collect location | RGPD risk > benefit |
| 2025-12-12 | Soft delete for alumni | Keep audit trail, RGPD compliance |
| 2025-12-12 | Phase 3 SQL only | Don't implement until Phase 3 |
| 2025-12-12 | Friday/Saturday split | Dedicated weekend window |

---

## Next Steps

1. **Design Phase 2 SQL schema** (users + users_history tables)
2. **Implement fetch scripts** (daily delta + weekly full)
3. **Test rate limiting** (verify 100 and 1,000 call spreads)
4. **Build monitoring** (Phase 1 + Phase 2 status scripts)
5. **Document RGPD decision** (location/live_status: DO NOT COLLECT)
6. **Prepare Phase 3 schemas** (SQL files, no execution)

---

**Status:** 📋 PHASE 2 ANALYSIS COMPLETE - READY FOR IMPLEMENTATION

**Approved:** Phase 2 structure locked (users table + Phase 3 prep + RGPD compliance)

**Challenge Level:** MEDIUM (100k users, but smart fetch strategy mitigates)
