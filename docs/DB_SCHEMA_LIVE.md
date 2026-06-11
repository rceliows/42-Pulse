# DB Schema (Live)

Generated from running PostgreSQL container 'transcendence_db' at **2026-03-11T20:30:01Z** (UTC).

## Tables

- `achievements`
- `achievements_delta`
- `achievements_users`
- `campus_achievements`
- `campus_achievements_delta`
- `campus_projects`
- `campus_projects_delta`
- `campuses`
- `campuses_delta`
- `coalitions`
- `coalitions_delta`
- `coalitions_users`
- `cursus`
- `cursus_delta`
- `delta_users`
- `detector_events`
- `project_sessions`
- `project_sessions_delta`
- `project_users`
- `projects`
- `projects_delta`
- `users`

## `achievements`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `text` | YES | `` |
| `description` | `text` | YES | `` |
| `tier` | `text` | YES | `` |
| `kind` | `text` | YES | `` |
| `visible` | `boolean` | YES | `` |
| `image` | `text` | YES | `` |
| `nbr_of_success` | `integer` | YES | `` |
| `users_url` | `text` | YES | `` |
| `parent_id` | `bigint` | YES | `` |
| `title` | `text` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX achievements_pkey ON public.achievements USING btree (id)
```

## `achievements_delta`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `text` | YES | `` |
| `description` | `text` | YES | `` |
| `tier` | `text` | YES | `` |
| `kind` | `text` | YES | `` |
| `visible` | `boolean` | YES | `` |
| `image` | `text` | YES | `` |
| `nbr_of_success` | `integer` | YES | `` |
| `users_url` | `text` | YES | `` |
| `parent_id` | `bigint` | YES | `` |
| `title` | `text` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
```

## `achievements_users`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `user_id` | `bigint` | NO | `` |
| `achievement_id` | `bigint` | NO | `` |
| `campus_id` | `bigint` | YES | `` |
| `user_login` | `text` | YES | `` |
| `user_email` | `text` | YES | `` |
| `name` | `text` | YES | `` |
| `kind` | `text` | YES | `` |
| `description` | `text` | YES | `` |
| `image` | `text` | YES | `` |
| `tier` | `text` | YES | `` |
| `nbr_of_success` | `integer` | YES | `` |
| `visible` | `boolean` | YES | `` |
| `users_url` | `text` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX achievements_users_pkey ON public.achievements_users USING btree (user_id, achievement_id)
CREATE INDEX idx_achievements_users_achievement_id ON public.achievements_users USING btree (achievement_id)
CREATE INDEX idx_achievements_users_campus_id ON public.achievements_users USING btree (campus_id)
CREATE INDEX idx_achievements_users_user_id ON public.achievements_users USING btree (user_id)
CREATE INDEX idx_achievements_users_kind ON public.achievements_users USING btree (kind)
```

## `campus_achievements`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `campus_id` | `bigint` | NO | `` |
| `achievement_id` | `bigint` | NO | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX campus_achievements_pkey ON public.campus_achievements USING btree (campus_id, achievement_id)
CREATE INDEX idx_campus_achievements_achievement ON public.campus_achievements USING btree (achievement_id)
```

## `campus_achievements_delta`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `campus_id` | `bigint` | NO | `` |
| `achievement_id` | `bigint` | NO | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
```

## `campus_projects`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `campus_id` | `bigint` | NO | `` |
| `project_id` | `bigint` | NO | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX campus_projects_pkey ON public.campus_projects USING btree (campus_id, project_id)
CREATE INDEX idx_campus_projects_project ON public.campus_projects USING btree (project_id)
```

## `campus_projects_delta`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `campus_id` | `bigint` | NO | `` |
| `project_id` | `bigint` | NO | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX campus_projects_delta_pkey ON public.campus_projects_delta USING btree (campus_id, project_id)
```

## `campuses`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `text` | YES | `` |
| `time_zone` | `text` | YES | `` |
| `language_id` | `bigint` | YES | `` |
| `language_name` | `text` | YES | `` |
| `language_identifier` | `text` | YES | `` |
| `users_count` | `integer` | YES | `` |
| `vogsphere_id` | `bigint` | YES | `` |
| `country` | `text` | YES | `` |
| `address` | `text` | YES | `` |
| `zip` | `text` | YES | `` |
| `city` | `text` | YES | `` |
| `website` | `text` | YES | `` |
| `facebook` | `text` | YES | `` |
| `twitter` | `text` | YES | `` |
| `public` | `boolean` | YES | `` |
| `active` | `boolean` | YES | `` |
| `email_extension` | `text` | YES | `` |
| `default_hidden_phone` | `boolean` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX campuses_pkey ON public.campuses USING btree (id)
CREATE INDEX idx_campuses_active ON public.campuses USING btree (active)
CREATE INDEX idx_campuses_city ON public.campuses USING btree (city)
CREATE INDEX idx_campuses_public ON public.campuses USING btree (public)
```

## `campuses_delta`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `text` | YES | `` |
| `time_zone` | `text` | YES | `` |
| `language_id` | `bigint` | YES | `` |
| `language_name` | `text` | YES | `` |
| `language_identifier` | `text` | YES | `` |
| `users_count` | `integer` | YES | `` |
| `vogsphere_id` | `bigint` | YES | `` |
| `country` | `text` | YES | `` |
| `address` | `text` | YES | `` |
| `zip` | `text` | YES | `` |
| `city` | `text` | YES | `` |
| `website` | `text` | YES | `` |
| `facebook` | `text` | YES | `` |
| `twitter` | `text` | YES | `` |
| `public` | `boolean` | YES | `` |
| `active` | `boolean` | YES | `` |
| `email_extension` | `text` | YES | `` |
| `default_hidden_phone` | `boolean` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
```

## `coalitions`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `character varying(255)` | NO | `` |
| `slug` | `character varying(255)` | NO | `` |
| `image_url` | `text` | YES | `` |
| `cover_url` | `text` | YES | `` |
| `color` | `character varying(7)` | YES | `` |
| `score` | `integer` | YES | `0` |
| `user_id` | `bigint` | YES | `` |
| `created_at` | `timestamp with time zone` | NO | `CURRENT_TIMESTAMP` |
| `updated_at` | `timestamp with time zone` | NO | `CURRENT_TIMESTAMP` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX coalitions_pkey ON public.coalitions USING btree (id)
CREATE INDEX idx_coalitions_score ON public.coalitions USING btree (score)
CREATE INDEX idx_coalitions_slug ON public.coalitions USING btree (slug)
CREATE INDEX idx_coalitions_user_id ON public.coalitions USING btree (user_id)
```

## `coalitions_delta`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `character varying(255)` | NO | `` |
| `slug` | `character varying(255)` | NO | `` |
| `image_url` | `text` | YES | `` |
| `cover_url` | `text` | YES | `` |
| `color` | `character varying(7)` | YES | `` |
| `score` | `integer` | YES | `0` |
| `user_id` | `bigint` | YES | `` |
| `created_at` | `timestamp with time zone` | NO | `CURRENT_TIMESTAMP` |
| `updated_at` | `timestamp with time zone` | NO | `CURRENT_TIMESTAMP` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
```

## `coalitions_users`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `coalition_id` | `bigint` | NO | `` |
| `user_id` | `bigint` | NO | `` |
| `score` | `integer` | YES | `0` |
| `rank` | `integer` | YES | `` |
| `campus_id` | `bigint` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX coalitions_users_pkey ON public.coalitions_users USING btree (id)
CREATE INDEX idx_coalitions_users_campus_id ON public.coalitions_users USING btree (campus_id)
CREATE INDEX idx_coalitions_users_coalition_id ON public.coalitions_users USING btree (coalition_id)
CREATE UNIQUE INDEX idx_coalitions_users_unique ON public.coalitions_users USING btree (coalition_id, user_id)
CREATE INDEX idx_coalitions_users_user_id ON public.coalitions_users USING btree (user_id)
```

## `cursus`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `text` | YES | `` |
| `slug` | `text` | YES | `` |
| `kind` | `text` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX cursus_pkey ON public.cursus USING btree (id)
CREATE INDEX idx_cursus_slug ON public.cursus USING btree (slug)
```

## `cursus_delta`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `text` | YES | `` |
| `slug` | `text` | YES | `` |
| `kind` | `text` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
```

## `delta_users`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | YES | `` |
| `email` | `text` | YES | `` |
| `login` | `text` | YES | `` |
| `first_name` | `text` | YES | `` |
| `last_name` | `text` | YES | `` |
| `usual_full_name` | `text` | YES | `` |
| `usual_first_name` | `text` | YES | `` |
| `url` | `text` | YES | `` |
| `phone` | `text` | YES | `` |
| `displayname` | `text` | YES | `` |
| `kind` | `text` | YES | `` |
| `image_link` | `text` | YES | `` |
| `image_large` | `text` | YES | `` |
| `image_medium` | `text` | YES | `` |
| `image_small` | `text` | YES | `` |
| `image_micro` | `text` | YES | `` |
| `image` | `jsonb` | YES | `` |
| `staff` | `boolean` | YES | `` |
| `correction_point` | `integer` | YES | `` |
| `pool_month` | `text` | YES | `` |
| `pool_year` | `text` | YES | `` |
| `location` | `text` | YES | `` |
| `wallet` | `integer` | YES | `` |
| `anonymize_date` | `timestamp with time zone` | YES | `` |
| `data_erasure_date` | `timestamp with time zone` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `alumnized_at` | `timestamp with time zone` | YES | `` |
| `alumni` | `boolean` | YES | `` |
| `active` | `boolean` | YES | `` |
| `cursus_id` | `bigint` | YES | `` |
| `kind_id` | `integer` | YES | `` |

### Indexes

```sql
```

## `detector_events`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `nextval('detector_events_id_seq'::regclass)` |
| `event_uid` | `character(32)` | NO | `` |
| `raw_line` | `text` | NO | `` |
| `source` | `text` | NO | `'detector'::text` |
| `ts` | `bigint` | YES | `` |
| `event_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `user_id` | `bigint` | YES | `` |
| `user_login` | `text` | YES | `` |
| `campus_id` | `bigint` | YES | `` |
| `first_snapshot` | `boolean` | YES | `` |
| `event_types` | `jsonb` | NO | `'[]'::jsonb` |
| `changes` | `jsonb` | NO | `'[]'::jsonb` |
| `payload` | `jsonb` | NO | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE UNIQUE INDEX detector_events_event_uid_key ON public.detector_events USING btree (event_uid)
CREATE UNIQUE INDEX detector_events_pkey ON public.detector_events USING btree (id)
CREATE INDEX idx_detector_events_campus_id ON public.detector_events USING btree (campus_id)
CREATE INDEX idx_detector_events_event_at ON public.detector_events USING btree (event_at DESC)
CREATE INDEX idx_detector_events_event_types_gin ON public.detector_events USING gin (event_types)
CREATE INDEX idx_detector_events_ingested_at ON public.detector_events USING btree (ingested_at DESC)
CREATE INDEX idx_detector_events_user_id ON public.detector_events USING btree (user_id)
```

## `project_sessions`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `project_id` | `bigint` | NO | `` |
| `campus_id` | `bigint` | YES | `` |
| `cursus_id` | `bigint` | YES | `` |
| `begin_at` | `timestamp with time zone` | YES | `` |
| `end_at` | `timestamp with time zone` | YES | `` |
| `difficulty` | `integer` | YES | `` |
| `estimate_time` | `text` | YES | `` |
| `exam` | `boolean` | YES | `` |
| `marked` | `boolean` | YES | `` |
| `max_project_submissions` | `integer` | YES | `` |
| `max_people` | `integer` | YES | `` |
| `duration_days` | `integer` | YES | `` |
| `commit` | `text` | YES | `` |
| `description` | `text` | YES | `` |
| `is_subscriptable` | `boolean` | YES | `` |
| `objectives` | `jsonb` | YES | `` |
| `scales` | `jsonb` | YES | `` |
| `terminating_after` | `text` | YES | `` |
| `uploads` | `jsonb` | YES | `` |
| `solo` | `boolean` | YES | `` |
| `team_behaviour` | `text` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE INDEX idx_project_sessions_campus_id ON public.project_sessions USING btree (campus_id)
CREATE INDEX idx_project_sessions_cursus_id ON public.project_sessions USING btree (cursus_id)
CREATE INDEX idx_project_sessions_project_id ON public.project_sessions USING btree (project_id)
CREATE UNIQUE INDEX project_sessions_pkey ON public.project_sessions USING btree (id)
```

## `project_sessions_delta`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `project_id` | `bigint` | NO | `` |
| `campus_id` | `bigint` | YES | `` |
| `cursus_id` | `bigint` | YES | `` |
| `begin_at` | `timestamp with time zone` | YES | `` |
| `end_at` | `timestamp with time zone` | YES | `` |
| `difficulty` | `integer` | YES | `` |
| `estimate_time` | `text` | YES | `` |
| `exam` | `boolean` | YES | `` |
| `marked` | `boolean` | YES | `` |
| `max_project_submissions` | `integer` | YES | `` |
| `max_people` | `integer` | YES | `` |
| `duration_days` | `integer` | YES | `` |
| `commit` | `text` | YES | `` |
| `description` | `text` | YES | `` |
| `is_subscriptable` | `boolean` | YES | `` |
| `objectives` | `jsonb` | YES | `` |
| `scales` | `jsonb` | YES | `` |
| `terminating_after` | `text` | YES | `` |
| `uploads` | `jsonb` | YES | `` |
| `solo` | `boolean` | YES | `` |
| `team_behaviour` | `text` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
```

## `project_users`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `project_id` | `bigint` | NO | `` |
| `campus_id` | `bigint` | YES | `` |
| `user_id` | `bigint` | NO | `` |
| `user_login` | `text` | YES | `` |
| `user_email` | `text` | YES | `` |
| `final_mark` | `integer` | YES | `` |
| `status` | `text` | YES | `` |
| `validated` | `boolean` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `cursus_ids` | `jsonb` | YES | `` |
| `current_team_id` | `bigint` | YES | `` |
| `marked` | `boolean` | YES | `` |
| `marked_at` | `timestamp with time zone` | YES | `` |
| `retriable_at` | `timestamp with time zone` | YES | `` |
| `occurrence` | `integer` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE INDEX idx_project_users_campus_id ON public.project_users USING btree (campus_id)
CREATE INDEX idx_project_users_marked ON public.project_users USING btree (marked)
CREATE INDEX idx_project_users_marked_at ON public.project_users USING btree (marked_at DESC)
CREATE INDEX idx_project_users_project_id ON public.project_users USING btree (project_id)
CREATE INDEX idx_project_users_status ON public.project_users USING btree (status)
CREATE INDEX idx_project_users_user_id ON public.project_users USING btree (user_id)
CREATE UNIQUE INDEX project_users_pkey ON public.project_users USING btree (id)
```

## `projects`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `text` | YES | `` |
| `slug` | `text` | YES | `` |
| `parent_id` | `bigint` | YES | `` |
| `difficulty` | `integer` | YES | `` |
| `exam` | `boolean` | YES | `` |
| `git_id` | `bigint` | YES | `` |
| `repository` | `text` | YES | `` |
| `recommendation` | `text` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE INDEX idx_projects_parent_id ON public.projects USING btree (parent_id)
CREATE INDEX idx_projects_slug ON public.projects USING btree (slug)
CREATE UNIQUE INDEX projects_pkey ON public.projects USING btree (id)
```

## `projects_delta`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `name` | `text` | YES | `` |
| `slug` | `text` | YES | `` |
| `parent_id` | `bigint` | YES | `` |
| `difficulty` | `integer` | YES | `` |
| `exam` | `boolean` | YES | `` |
| `git_id` | `bigint` | YES | `` |
| `repository` | `text` | YES | `` |
| `recommendation` | `text` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
```

## `users`

### Columns

| Column | Type | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` | NO | `` |
| `email` | `text` | YES | `` |
| `login` | `text` | YES | `` |
| `first_name` | `text` | YES | `` |
| `last_name` | `text` | YES | `` |
| `usual_full_name` | `text` | YES | `` |
| `usual_first_name` | `text` | YES | `` |
| `url` | `text` | YES | `` |
| `phone` | `text` | YES | `` |
| `displayname` | `text` | YES | `` |
| `kind` | `text` | YES | `` |
| `image_link` | `text` | YES | `` |
| `image_large` | `text` | YES | `` |
| `image_medium` | `text` | YES | `` |
| `image_small` | `text` | YES | `` |
| `image_micro` | `text` | YES | `` |
| `image` | `jsonb` | YES | `` |
| `staff` | `boolean` | YES | `` |
| `correction_point` | `integer` | YES | `` |
| `pool_month` | `text` | YES | `` |
| `pool_year` | `text` | YES | `` |
| `location` | `text` | YES | `` |
| `wallet` | `integer` | YES | `` |
| `anonymize_date` | `timestamp with time zone` | YES | `` |
| `data_erasure_date` | `timestamp with time zone` | YES | `` |
| `created_at` | `timestamp with time zone` | YES | `` |
| `updated_at` | `timestamp with time zone` | YES | `` |
| `alumnized_at` | `timestamp with time zone` | YES | `` |
| `alumni` | `boolean` | YES | `` |
| `active` | `boolean` | YES | `` |
| `campus_id` | `bigint` | YES | `` |
| `ingested_at` | `timestamp with time zone` | NO | `now()` |

### Indexes

```sql
CREATE INDEX idx_users_active ON public.users USING btree (active)
CREATE INDEX idx_users_campus_id ON public.users USING btree (campus_id)
CREATE INDEX idx_users_email ON public.users USING btree (email)
CREATE INDEX idx_users_kind ON public.users USING btree (kind)
CREATE INDEX idx_users_login ON public.users USING btree (login)
CREATE INDEX idx_users_updated_at ON public.users USING btree (updated_at)
CREATE UNIQUE INDEX users_pkey ON public.users USING btree (id)
```
