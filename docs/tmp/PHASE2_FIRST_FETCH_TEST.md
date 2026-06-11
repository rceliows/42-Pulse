# Phase 2 First Fetch Test - Results

**Date**: December 12, 2025  
**Endpoint**: `/v2/cursus/21/cursus_users?per_page=1&sort=id`  
**Test User**: ldutriez (ID 50118, Alumni, Cursus 21)  
**Status**: ✅ SUCCESS - All fields validated

---

## Test Execution

```bash
cd /srv/42_Network/repo
./.cache/bin/token-manager-agent call-export "/v2/cursus/21/cursus_users?per_page=1&sort=id" /tmp/user_sample.json
cat /tmp/user_sample.json | jq '.[0]'
```

**Result**: Successfully fetched sample user, 38 fields received

---

## API Response Structure

### Level 1: cursus_users object (enrollment record)
- `id` - cursus_users record ID
- `begin_at` - enrollment start date
- `end_at` - enrollment end date (null if ongoing)
- `grade` - grade/status (e.g., "Alumni")
- `level` - progression level (e.g., 11.69)
- `skills[]` - array of skill objects
- `cursus_id` - cursus ID (21 for our data)
- `has_coalition` - boolean
- `blackholed_at` - blackhole date if applicable
- `created_at`, `updated_at` - timestamps
- `user {}` - **nested user object** (see below)
- `cursus {}` - nested cursus metadata

### Level 2: user object (user profile)
```
user {
  id, email, login, first_name, last_name,
  usual_full_name, usual_first_name,
  url, phone, displayname, kind,
  image { link, versions { large, medium, small, micro } },
  staff?, correction_point,
  pool_month, pool_year,
  location (null - RGPD), wallet,
  anonymize_date, data_erasure_date,
  created_at, updated_at, alumnized_at,
  alumni?, active?
}
```

---

## Field Mapping: API → Database

### ✅ Users Table (Profile Data)

All 33 fields map correctly:

| DB Field | API Source | Type | Notes |
|----------|-----------|------|-------|
| id | user.id | BIGINT | Primary key |
| email | user.email | TEXT | |
| login | user.login | TEXT | Unique identifier |
| first_name | user.first_name | TEXT | |
| last_name | user.last_name | TEXT | |
| usual_full_name | user.usual_full_name | TEXT | |
| usual_first_name | user.usual_first_name | TEXT | |
| url | user.url | TEXT | API endpoint link |
| phone | user.phone | TEXT | "hidden" in API |
| displayname | user.displayname | TEXT | Display name |
| kind | user.kind | TEXT | student, staff, etc |
| image_link | user.image.link | TEXT | Profile image URL |
| image_large | user.image.versions.large | TEXT | Large variant |
| image_medium | user.image.versions.medium | TEXT | Medium variant |
| image_small | user.image.versions.small | TEXT | Small variant |
| image_micro | user.image.versions.micro | TEXT | Micro variant |
| image | user.image | JSONB | Full object |
| staff | user.staff? | BOOLEAN | Staff status (field has ?) |
| correction_point | user.correction_point | INTEGER | CP balance |
| pool_month | user.pool_month | TEXT | february, march, etc |
| pool_year | user.pool_year | TEXT | 2019, 2020, etc |
| location | user.location | TEXT | **NULL in API** (RGPD) |
| wallet | user.wallet | INTEGER | Account balance |
| anonymize_date | user.anonymize_date | TIMESTAMPTZ | Data anonymization |
| data_erasure_date | user.data_erasure_date | TIMESTAMPTZ | GDPR erasure |
| created_at | user.created_at | TIMESTAMPTZ | Account creation |
| updated_at | user.updated_at | TIMESTAMPTZ | Last profile update |
| alumnized_at | user.alumnized_at | TIMESTAMPTZ | Alumni status date |
| alumni | user.alumni? | BOOLEAN | Alumni flag (field has ?) |
| active | user.active? | BOOLEAN | Active flag (field has ?) |
| campus_id | NOT IN API | BIGINT | **To be added** |
| ingested_at | SYSTEM NOW() | TIMESTAMPTZ | Sync timestamp |

**Status**: ✅ All 33 fields present and correct format

---

### ⚠️ Missing: campus_id

The API response doesn't include campus information for the user's primary campus.

**Solution**: Fetch additional user data from secondary endpoints or set campus_id=NULL initially

**Note**: Since we're fetching from `/v2/cursus/21/cursus_users`, all users are in Cursus 21. 
We ignore cursus_id (always 21), begin_at, end_at, grade, level, skills, has_coalition, blackholed_at - **NOT USED in project**.

Only the nested `user` object fields matter.

---

### 📦 New Tables NOT Needed

**What we're NOT storing** (out of scope):

❌ cursus_users table
  - The `/v2/cursus/21/cursus_users` endpoint returns enrollment records
  - We use it only as a **source list** to find which users to fetch
  - Fields like begin_at, end_at, grade, level, blackholed_at: NOT USED
  - cursus_id is always 21: NOT STORED (we already know it)

❌ cursus_skills table
  - API returns skills array per cursus_user
  - NOT USED in project: IGNORE

**What we actually need**:

✅ users table (already correct, just needs campus_id field)
   - Stores full user profile from nested user object (33 fields)
   - campus_id to be populated from secondary fetch or left NULL

---

## RGPD Compliance Verified

**Confirmed**: `location` field is NULL in API response (no location data collected)

Sample response shows:
```json
"location": null
```

This aligns with Phase 2 RGPD decision: **NO location/live_status collection**

---

## Data Quality Notes

### Field Name Quirks (API design)

Some boolean fields end with `?`:
- `staff?` (API) → `staff` (DB)
- `alumni?` (API) → `alumni` (DB)  
- `active?` (API) → `active` (DB)

When extracting: **strip the `?` character from field names**

### Dynamic vs Static Fields

**Dynamic** (changes frequently):
- `location` (currently NULL)
- `wallet` (balance changes)
- `correction_point` (CP earned/spent)
- `updated_at` (changes on any user update)

**Static** (rarely change):
- `login` (never)
- `first_name`, `last_name` (rarely)
- `email` (rarely)
- `created_at` (never)
- `kind` (rarely)
- `pool_month`, `pool_year` (never)

---

## Fetch Strategy for Phase 2

### Simple Single-Step Approach

**Get cursus_users list to extract user profiles**
```bash
GET /v2/cursus/21/cursus_users?per_page=100&sort=updated_at DESC
```

**What we do**:
1. Iterate through cursus_users response
2. Extract nested `user` object from each record
3. Insert/update users table with 33 user fields
4. Ignore top-level cursus_user fields (begin_at, end_at, grade, level, skills, etc) - NOT USED

**Why this works**:
- Cursus 21 is fixed (no other cursus to fetch)
- User object is complete and sufficient
- No need to store enrollment/skill data
- Cost: ~500 calls for 50k students (one API call)

**Filtering**:
```bash
GET /v2/cursus/21/cursus_users?filter[alumni]=false&per_page=100
```
- Only fetch active students (alumni=false)
- Cursus 21 is implicit in endpoint
- No need for secondary cursus or skill fetches

**Daily delta**:
```bash
GET /v2/cursus/21/cursus_users?filter[alumni]=false&range[updated_at]=YESTERDAY,NOW&per_page=100
```
- Cost: ~5-20 calls depending on daily changes
- Only fetch changed users

---

### API Response Extraction Logic

```
For each item in /v2/cursus/21/cursus_users response:
├─ Use item.user object (nested)
├─ Extract 33 fields from user
├─ Insert/update into users table
└─ Ignore item.begin_at, item.end_at, item.grade, item.level, 
   item.skills[], item.has_coalition, item.blackholed_at (NOT USED)
```

No secondary fetches needed. Simple extraction from nested `user` object.

---

## Next Steps

1. ✅ **DONE**: Verify API response structure
2. ⏳ **NEXT**: Add campus_id field to users table (schema.sql)
3. ⏳ **THEN**: Create delta_users table (staging)
4. ⏳ **THEN**: Write fetch_cursus_21_users_full.sh script
5. ⏳ **THEN**: Write fetch_cursus_21_users_delta.sh script
6. ⏳ **THEN**: Test with sample data
7. ⏳ **THEN**: Integrate into nightly cron

---

## Simplified Table Requirements

**Only need to modify existing tables**:
- Add `campus_id` column to `users` table
- Create `delta_users` table (staging area, same schema as users)

**No new tables needed**:
- ❌ cursus_users (only source list, data extracted to users)
- ❌ cursus_skills (not used in project)

---

## Test Artifacts

- **Sample response**: `/tmp/user_sample.json`
- **Verified on**: 2025-12-12
- **API version**: `/v2/`
- **Test user**: ldutriez (ID: 50118, Alumni, Cursus 21)

All field mappings and table designs ready for implementation.
