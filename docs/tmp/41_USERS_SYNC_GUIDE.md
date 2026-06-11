# Users Table Sync - Simple Setup

**Status**: ✅ Ready  
**Data**: 40,335 students (cursus 21)  
**Update Frequency**: Configurable (every X hours)  
**Pattern**: Simple UPSERT - no deltas, just update changed records

---

## Architecture

```
API (42.fr)
    ↓
toolkit-agent fetch_cursus_21_users_simple
    ↓
exports/08_users/all.json (40k users)
    ↓
toolkit-agent update_users_simple (UPSERT to DB)
    ↓
users table (40,335 rows)
    ↓
Logging to logs/update_users.log
```

---

## Scripts

### 1. Fetch Script
**File**: `toolkit-agent fetch_cursus_21_users_simple`

Fetches all cursus 21 students with pagination:
- Filter: `kind=student&alumni?=false`
- Per page: 100 users
- Output: `exports/08_users/all.json`
- Metadata: `.last_fetch_epoch`, `.last_fetch_stats`

```bash
./.cache/bin/toolkit-agent fetch_cursus_21_users_simple
```

### 2. Update Script
**Command**: `toolkit-agent update_users_simple`

Loads JSON into database:
- Creates `users` table if not exists
- Stages into `users_delta` (temp)
- UPSERT: insert if new, update if changed
- Updates: email, login, pool info, location, wallet, updated_at
- Indexes: login, email, updated_at

```bash
./.cache/bin/toolkit-agent update_users_simple
```

### 3. Cron Wrapper
**Command**: `toolkit-agent sync_users_rolling`

Runs both fetch + update in sequence:
```bash
./.cache/bin/toolkit-agent sync_users_rolling
```

---

## Usage

### Manual Sync (One-time)
```bash
cd /srv/42_Network/repo
./.cache/bin/toolkit-agent sync_users_rolling
```

### Scheduled Sync (Every 6 hours)
Add to crontab:
```bash
crontab -e

# Add this line:
0 */6 * * * /srv/42_Network/repo/.cache/bin/toolkit-agent sync_users_rolling >> /srv/42_Network/runtime/logs/update_users.log 2>&1
```

### Every 4 hours
```bash
0 */4 * * * /srv/42_Network/repo/.cache/bin/toolkit-agent sync_users_rolling >> /srv/42_Network/runtime/logs/update_users.log 2>&1
```

### Every 12 hours (2x/day)
```bash
0 0,12 * * * /srv/42_Network/repo/.cache/bin/toolkit-agent sync_users_rolling >> /srv/42_Network/runtime/logs/update_users.log 2>&1
```

---

## Current Database

**Table**: `users`  
**Records**: 40,335 (loaded from v2 snapshot)  
**Last Updated**: 2025-12-13T14:30:00Z  
**Schema**: 30 columns (id, email, login, names, pool, location, wallet, correction_point, timestamps, etc.)

---

## What Gets Updated

- ✅ `email`, `login`, `first_name`, `last_name`
- ✅ `pool_month`, `pool_year`
- ✅ `location` (if changed)
- ✅ `wallet`, `correction_point`
- ✅ `updated_at` (tracks last API refresh)
- ✅ `ingested_at` (tracks DB insertion time)

---

## Logs

Check sync results:
```bash
tail -f logs/update_users.log
tail -f logs/fetch_cursus_21_users.log
```

Expected output:
```
[2025-12-13T14:31:00Z] Starting users update...
[2025-12-13T14:31:15Z] Staged 40335 users
[2025-12-13T14:31:45Z] Users: total=40335
[2025-12-13T14:31:45Z] Update complete
```

---

## Next Steps

1. ✅ Scripts created & tested
2. ⏳ Add to crontab (choose frequency)
3. ⏳ Monitor logs for 24h
4. ⏳ Phase 3: nested data (achievements_users, projects_users, coalitions_users)
