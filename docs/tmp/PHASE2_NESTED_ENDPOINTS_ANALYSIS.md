# Phase 2: Nested Endpoints Analysis - J-to-N Tables

**Date**: December 12, 2025  
**Status**: ✅ Nested data confirmed in user object  
**Scope**: Cursus 21 users with related data

---

## Nested Data Available in `/v2/users/{id}` Response

The user detail endpoint returns COMPLETE nested objects:

### ✅ Available Nested Arrays

| Field | Type | Content | Count | Use |
|-------|------|---------|-------|-----|
| `achievements[]` | Array | Achievement metadata | ~50+ | → `achievements_users` table |
| `projects_users[]` | Array | Project enrollment/marks | ~50+ | → `projects_users` table |
| `campus[]` | Array | Campus(es) of user | 1-2 | → Extract campus_id |
| `cursus_users[]` | Array | Cursus enrollment history | 2+ | Filter to Cursus 21 |
| `groups[]` | Array | User groups/memberships | 2-10 | Not used |
| `titles_users[]` | Array | Titles earned | varies | Not used |
| `expertises_users[]` | Array | Expertise ratings | varies | Not used |
| `languages_users[]` | Array | Languages spoken | varies | Not used |
| `partnerships[]` | Array | Partnerships | varies | Not used |
| `roles[]` | Array | User roles | varies | Not used |
| `patroned[]` | Array | Mentoring relationships | varies | Not used |
| `patroning[]` | Array | Mentoring relationships | varies | Not used |
| `campus_users[]` | Array | Campus enrollment records | ? | Not checked |

---

## ❌ NOT Available: coalitions_users

**Coalitions_users is NOT in the user detail object**.

Options to get coalitions for a user:
1. Separate fetch: `/v2/coalitions/{id}/coalitions_users?filter[user_id]=50118`
2. Fetch from `/v2/users/{id}/coalitions` (if it exists)
3. Store coalition assignments in different phase

**For Phase 2**: SKIP coalitions_users, focus on users + achievements_users + projects_users

---

## Data Structures

### achievements[] (nested in user)

```json
{
  "id": 40,
  "name": "404 - Sleep not found",
  "description": "Logged for 24h straight",
  "tier": "easy",
  "kind": "scolarity",
  "visible": true,
  "image": "/uploads/achievement/image/40/SCO001.svg",
  "nbr_of_success": null,
  "users_url": "https://api.intra.42.fr/v2/achievements/40/users"
}
```

**For achievements_users table**:
- Extract: achievement.id, user.id
- Insert: `(user_id, achievement_id, ingested_at)`
- Simple junction table

### projects_users[] (nested in user)

```json
{
  "id": 3046408,
  "occurrence": 0,
  "final_mark": 100,
  "status": "finished",
  "validated?": true,
  "current_team_id": 4781868,
  "project": {
    "id": 2076,
    "name": "ready set boole",
    "slug": "ready-set-boole",
    "parent_id": null
  },
  "cursus_ids": [21],
  "marked_at": "2023-03-27T17:13:21.198Z",
  "marked": true,
  "retriable_at": "2023-03-30T17:13:21.614Z",
  "created_at": "2023-03-27T13:54:34.075Z",
  "updated_at": "2023-03-27T17:13:21.631Z"
}
```

**For projects_users table**:
- Extract from projects_users array item:
  - id, user_id, project.id, occurrence, final_mark, status, validated?, 
    current_team_id, marked_at, marked, retriable_at, created_at, updated_at
- Filter: Only if 21 in cursus_ids (to include only Cursus 21 projects)

### campus[] (nested in user)

```json
[
  {
    "id": 1,
    "name": "Paris",
    "time_zone": "Europe/Paris",
    "users_count": 39708,
    ...
  }
]
```

**For users table**:
- Extract: campus[0].id → set as campus_id
- If multiple campuses: use first one

---

## Phase 2 Data Population Strategy

### Step 1: Fetch users (from cursus_users endpoint)

```bash
GET /v2/cursus/21/cursus_users?filter[alumni]=false&per_page=100
```

For each item:
- Extract `item.user` → insert into `users` table (33 fields + campus_id)

### Step 2: Fetch detailed user profiles

```bash
GET /v2/users/{id}
```

For each user:
- Extract `achievements[]` → insert into `achievements_users` table
- Extract `projects_users[]` → filter for Cursus 21, insert into `projects_users` table
- Extract `campus[0]` → update users.campus_id

### Alternative Strategy (More Efficient)

If we fetch `/v2/users` with filter for Cursus 21 users:

```bash
GET /v2/users?filter[id]=id1,id2,...&per_page=100
```

Response includes nested achievements + projects_users + campus → populate all 3 tables at once

---

## Tables Required for Phase 2

### 1. users (modify existing)
- Add `campus_id` BIGINT column
- All 33 profile fields already present

### 2. achievements_users (NEW - junction table)
```sql
CREATE TABLE IF NOT EXISTS achievements_users (
  id                SERIAL PRIMARY KEY,
  user_id           BIGINT REFERENCES users(id),
  achievement_id    BIGINT REFERENCES achievements(id),
  ingested_at       TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE UNIQUE INDEX idx_achievements_users_unique ON achievements_users(user_id, achievement_id);
CREATE INDEX idx_achievements_users_user_id ON achievements_users(user_id);
```

### 3. projects_users (NEW - enrollment records)
```sql
CREATE TABLE IF NOT EXISTS projects_users (
  id                BIGINT PRIMARY KEY,
  user_id           BIGINT REFERENCES users(id),
  project_id        BIGINT REFERENCES projects(id),
  occurrence        INTEGER,
  final_mark        INTEGER,
  status            TEXT,  -- 'finished', 'in_progress', etc
  validated         BOOLEAN,  -- API field: validated?
  current_team_id   BIGINT,
  marked_at         TIMESTAMPTZ,
  marked            BOOLEAN,
  retriable_at      TIMESTAMPTZ,
  created_at        TIMESTAMPTZ,
  updated_at        TIMESTAMPTZ,
  ingested_at       TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_projects_users_user_id ON projects_users(user_id);
CREATE INDEX idx_projects_users_project_id ON projects_users(project_id);
```

### 4. delta_achievements_users (staging)
```sql
CREATE TABLE IF NOT EXISTS delta_achievements_users (
  id                SERIAL PRIMARY KEY,
  user_id           BIGINT,
  achievement_id    BIGINT,
  ingested_at       TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

### 5. delta_projects_users (staging)
```sql
CREATE TABLE IF NOT EXISTS delta_projects_users (
  id                BIGINT,
  user_id           BIGINT,
  project_id        BIGINT,
  occurrence        INTEGER,
  final_mark        INTEGER,
  status            TEXT,
  validated         BOOLEAN,
  current_team_id   BIGINT,
  marked_at         TIMESTAMPTZ,
  marked            BOOLEAN,
  retriable_at      TIMESTAMPTZ,
  created_at        TIMESTAMPTZ,
  updated_at        TIMESTAMPTZ,
  ingested_at       TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

### ❌ coalitions_users (DEFER)

Not available as nested data in `/v2/users`. Would require:
- Separate API fetches from `/v2/coalitions/{id}/coalitions_users`
- OR from each user's coalitions list (if available)

**Decision**: Handle in Phase 3 or separate task when needed

---

## Extraction Logic for Each Table

### users table (from /v2/cursus/21/cursus_users)

```
For each item in response:
  Extract item.user:
    → Insert/update users (33 fields from user + campus_id from campus[0].id)
```

### achievements_users table (from /v2/users/{id})

```
For each user:
  For each item in user.achievements[]:
    → Insert (user_id, achievement_id, NOW())
    → Ignore duplicate errors (some users have same achievement multiple times)
```

### projects_users table (from /v2/users/{id})

```
For each user:
  For each item in user.projects_users[]:
    If 21 in item.cursus_ids:
      → Insert (id, user_id, project.id, occurrence, final_mark, status, 
                validated, current_team_id, marked_at, marked, retriable_at, 
                created_at, updated_at)
```

---

## API Cost Analysis

**Initial Sync**:
- GET /v2/cursus/21/cursus_users: ~500 calls (50k users, 100/page)
- GET /v2/users/{id}: ~500 calls (one per user for detailed data)
- **Total: ~1000 calls** (within 1200/hour limit)

**Daily Delta**:
- GET /v2/cursus/21/cursus_users?range[updated_at]=...: ~5-20 calls
- GET /v2/users/{id} for changed users only: ~1-5 calls
- **Total: ~10-25 calls/day**

---

## Summary: Tables to Create

| Table | Type | Records | Source | Priority |
|-------|------|---------|--------|----------|
| users | EXISTS | +50k | /v2/cursus/21/cursus_users | MODIFY (add campus_id) |
| achievements_users | NEW | ~2.5M | /v2/users/{id}.achievements[] | ✅ Phase 2 |
| projects_users | NEW | ~500k | /v2/users/{id}.projects_users[] | ✅ Phase 2 |
| coalitions_users | NEW | ? | NOT IN NESTED DATA | ⏳ Phase 3 |

**Phase 2 Focus**: users + achievements_users + projects_users

Achievement: Extract all nested data from single user fetch, populate 3 tables!
