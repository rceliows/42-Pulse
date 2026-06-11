-- Lean schema: essential tables for live tracking
-- Core: campuses, users, cursus, projects, project_sessions
-- Live tracking: project_users (enrollments), achievements_users (earned badges)

CREATE TABLE IF NOT EXISTS achievements (
  id             BIGINT PRIMARY KEY,
  name           TEXT,
  description    TEXT,
  tier           TEXT,
  kind           TEXT,
  visible        BOOLEAN,
  image          TEXT,
  nbr_of_success INTEGER,
  users_url      TEXT,
  parent_id      BIGINT,
  title          TEXT,
  ingested_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS campuses (
  id           BIGINT PRIMARY KEY,
  name         TEXT,
  time_zone    TEXT,
  language_id  BIGINT,
  language_name TEXT,
  language_identifier TEXT,
  users_count  INTEGER,
  vogsphere_id BIGINT,
  country      TEXT,
  address      TEXT,
  zip          TEXT,
  city         TEXT,
  website      TEXT,
  facebook     TEXT,
  twitter      TEXT,
  public       BOOLEAN,
  active       BOOLEAN,
  email_extension       TEXT,
  default_hidden_phone  BOOLEAN,
  ingested_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_campuses_city ON campuses (city);
CREATE INDEX IF NOT EXISTS idx_campuses_active ON campuses (active);
CREATE INDEX IF NOT EXISTS idx_campuses_public ON campuses (public);

-- Users (base profile rows; campus scoped fetch recommended)
CREATE TABLE IF NOT EXISTS users (
  id                BIGINT PRIMARY KEY,
  email             TEXT,
  login             TEXT,
  first_name        TEXT,
  last_name         TEXT,
  usual_full_name   TEXT,
  usual_first_name  TEXT,
  url               TEXT,
  phone             TEXT,
  displayname       TEXT,
  kind              TEXT,
  image_link        TEXT,
  image_large       TEXT,
  image_medium      TEXT,
  image_small       TEXT,
  image_micro       TEXT,
  image             JSONB,
  staff             BOOLEAN,
  correction_point  INTEGER,
  pool_month        TEXT,
  pool_year         TEXT,
  location          TEXT,
  wallet            INTEGER,
  anonymize_date    TIMESTAMPTZ,
  data_erasure_date TIMESTAMPTZ,
  created_at        TIMESTAMPTZ,
  updated_at        TIMESTAMPTZ,
  alumnized_at      TIMESTAMPTZ,
  alumni            BOOLEAN,
  active            BOOLEAN,
  campus_id         BIGINT,
  ingested_at       TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_users_login ON users (login);
CREATE INDEX IF NOT EXISTS idx_users_email ON users (email);
CREATE INDEX IF NOT EXISTS idx_users_active ON users (active);
CREATE INDEX IF NOT EXISTS idx_users_kind ON users (kind);
CREATE INDEX IF NOT EXISTS idx_users_updated_at ON users (updated_at);
CREATE INDEX IF NOT EXISTS idx_users_campus_id ON users (campus_id);

-- Local application accounts for email/password authentication.
-- Passwords are stored as salted scrypt hashes; plaintext passwords are never stored.
CREATE TABLE IF NOT EXISTS app_users (
  id            BIGSERIAL PRIMARY KEY,
  email         TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  display_name  TEXT,
  role          TEXT NOT NULL DEFAULT 'user',
  created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_app_users_email ON app_users (email);

CREATE TABLE IF NOT EXISTS app_sessions (
  id          BIGSERIAL PRIMARY KEY,
  user_id     BIGINT NOT NULL REFERENCES app_users(id) ON DELETE CASCADE,
  token_hash  CHAR(64) NOT NULL UNIQUE,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  expires_at  TIMESTAMPTZ NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_app_sessions_user_id ON app_sessions (user_id);
CREATE INDEX IF NOT EXISTS idx_app_sessions_expires_at ON app_sessions (expires_at);

-- Canonical detector events (deduplicated history, separate from live JSONL queue)
CREATE TABLE IF NOT EXISTS detector_events (
  id             BIGSERIAL PRIMARY KEY,
  event_uid      CHAR(32) UNIQUE NOT NULL,
  raw_line       TEXT NOT NULL,
  source         TEXT NOT NULL DEFAULT 'detector',
  ts             BIGINT,
  event_at       TIMESTAMPTZ,
  updated_at     TIMESTAMPTZ,
  user_id        BIGINT,
  user_login     TEXT,
  campus_id      BIGINT,
  first_snapshot BOOLEAN,
  event_types    JSONB NOT NULL DEFAULT '[]'::jsonb,
  changes        JSONB NOT NULL DEFAULT '[]'::jsonb,
  payload        JSONB NOT NULL,
  ingested_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_detector_events_event_at ON detector_events (event_at DESC);
CREATE INDEX IF NOT EXISTS idx_detector_events_user_id ON detector_events (user_id);
CREATE INDEX IF NOT EXISTS idx_detector_events_campus_id ON detector_events (campus_id);
CREATE INDEX IF NOT EXISTS idx_detector_events_ingested_at ON detector_events (ingested_at DESC);
CREATE INDEX IF NOT EXISTS idx_detector_events_event_types_gin ON detector_events USING GIN (event_types);

CREATE TABLE IF NOT EXISTS cursus (
  id          BIGINT PRIMARY KEY,
  name        TEXT,
  slug        TEXT,
  kind        TEXT,
  created_at  TIMESTAMPTZ,
  ingested_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_cursus_slug ON cursus (slug);

CREATE TABLE IF NOT EXISTS projects (
  id          BIGINT PRIMARY KEY,
  name        TEXT,
  slug        TEXT,
  parent_id   BIGINT,
  difficulty  INTEGER,
  exam        BOOLEAN,
  git_id      BIGINT,
  repository  TEXT,
  recommendation TEXT,
  created_at  TIMESTAMPTZ,
  updated_at  TIMESTAMPTZ,
  ingested_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_projects_slug ON projects (slug);
CREATE INDEX IF NOT EXISTS idx_projects_parent_id ON projects (parent_id);

-- Project sessions (per campus/cursus)
CREATE TABLE IF NOT EXISTS project_sessions (
  id                      BIGINT PRIMARY KEY,
  project_id              BIGINT NOT NULL,
  campus_id               BIGINT,
  cursus_id               BIGINT,
  begin_at                TIMESTAMPTZ,
  end_at                  TIMESTAMPTZ,
  difficulty              INTEGER,
  estimate_time           TEXT,
  exam                    BOOLEAN,
  marked                  BOOLEAN,
  max_project_submissions INTEGER,
  max_people              INTEGER,
  duration_days           INTEGER,
  commit                  TEXT,
  description             TEXT,
  is_subscriptable        BOOLEAN,
  objectives              JSONB,
  scales                  JSONB,
  terminating_after       TEXT,
  uploads                 JSONB,
  solo                    BOOLEAN,
  team_behaviour          TEXT,
  created_at              TIMESTAMPTZ,
  updated_at              TIMESTAMPTZ,
  ingested_at             TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_project_sessions_project_id ON project_sessions (project_id);
CREATE INDEX IF NOT EXISTS idx_project_sessions_campus_id ON project_sessions (campus_id);
CREATE INDEX IF NOT EXISTS idx_project_sessions_cursus_id ON project_sessions (cursus_id);

-- Linker: which campuses offer which projects
CREATE TABLE IF NOT EXISTS campus_projects (
  campus_id     BIGINT NOT NULL,
  project_id    BIGINT NOT NULL,
  ingested_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  PRIMARY KEY (campus_id, project_id)
);

CREATE INDEX IF NOT EXISTS idx_campus_projects_project ON campus_projects (project_id);

-- Linker: which achievements are available at which campus
CREATE TABLE IF NOT EXISTS campus_achievements (
  campus_id       BIGINT NOT NULL,
  achievement_id  BIGINT NOT NULL,
  ingested_at     TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  PRIMARY KEY (campus_id, achievement_id)
);

CREATE INDEX IF NOT EXISTS idx_campus_achievements_achievement ON campus_achievements (achievement_id);

-- Project users (per campus/project) - tracks user enrollment and progress
CREATE TABLE IF NOT EXISTS project_users (
  id                  BIGINT PRIMARY KEY,
  project_id          BIGINT NOT NULL,
  campus_id           BIGINT,
  user_id             BIGINT NOT NULL,
  user_login          TEXT,
  user_email          TEXT,
  final_mark          INTEGER,
  status              TEXT,
  validated           BOOLEAN,
  created_at          TIMESTAMPTZ,
  updated_at          TIMESTAMPTZ,
  cursus_ids          JSONB,
  current_team_id     BIGINT,
  marked              BOOLEAN,
  marked_at           TIMESTAMPTZ,
  retriable_at        TIMESTAMPTZ,
  occurrence          INTEGER,
  ingested_at         TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_project_users_project_id ON project_users (project_id);
CREATE INDEX IF NOT EXISTS idx_project_users_user_id ON project_users (user_id);
CREATE INDEX IF NOT EXISTS idx_project_users_campus_id ON project_users (campus_id);
CREATE INDEX IF NOT EXISTS idx_project_users_status ON project_users (status);
CREATE INDEX IF NOT EXISTS idx_project_users_marked ON project_users (marked);
CREATE INDEX IF NOT EXISTS idx_project_users_marked_at ON project_users (marked_at DESC);

-- Achievements users (per campus) - tracks earned achievements/badges
CREATE TABLE IF NOT EXISTS achievements_users (
  user_id             BIGINT NOT NULL,
  achievement_id      BIGINT NOT NULL,
  campus_id           BIGINT,
  user_login          TEXT,
  user_email          TEXT,
  name                TEXT,
  kind                TEXT,
  description         TEXT,
  image               TEXT,
  tier                TEXT,
  nbr_of_success      INTEGER,
  visible             BOOLEAN,
  users_url           TEXT,
  created_at          TIMESTAMPTZ,
  updated_at          TIMESTAMPTZ,
  ingested_at         TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  PRIMARY KEY (user_id, achievement_id)
);

CREATE INDEX IF NOT EXISTS idx_achievements_users_achievement_id ON achievements_users (achievement_id);
CREATE INDEX IF NOT EXISTS idx_achievements_users_user_id ON achievements_users (user_id);
CREATE INDEX IF NOT EXISTS idx_achievements_users_campus_id ON achievements_users (campus_id);
CREATE INDEX IF NOT EXISTS idx_achievements_users_kind ON achievements_users (kind);

-- Coalitions (teams/groups per campus or global)
CREATE TABLE IF NOT EXISTS coalitions (
  id            BIGINT PRIMARY KEY,
  name          VARCHAR(255) NOT NULL,
  slug          VARCHAR(255) UNIQUE NOT NULL,
  image_url     TEXT,
  cover_url     TEXT,
  color         VARCHAR(7),
  score         INTEGER DEFAULT 0,
  user_id       BIGINT,
  created_at    TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at    TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  ingested_at   TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_coalitions_slug ON coalitions (slug);
CREATE INDEX IF NOT EXISTS idx_coalitions_user_id ON coalitions (user_id);
CREATE INDEX IF NOT EXISTS idx_coalitions_score ON coalitions (score);

-- Coalitions users (membership + scores/ranks)
CREATE TABLE IF NOT EXISTS coalitions_users (
  id            BIGINT PRIMARY KEY,
  coalition_id  BIGINT NOT NULL,
  user_id       BIGINT NOT NULL,
  score         INTEGER DEFAULT 0,
  rank          INTEGER,
  campus_id     BIGINT,
  created_at    TIMESTAMPTZ,
  updated_at    TIMESTAMPTZ,
  ingested_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  CONSTRAINT fk_coalitions_users_coalition
    FOREIGN KEY (coalition_id) REFERENCES coalitions(id) ON DELETE CASCADE,
  CONSTRAINT fk_coalitions_users_user
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_coalitions_users_coalition_id ON coalitions_users (coalition_id);
CREATE INDEX IF NOT EXISTS idx_coalitions_users_user_id ON coalitions_users (user_id);
CREATE INDEX IF NOT EXISTS idx_coalitions_users_campus_id ON coalitions_users (campus_id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_coalitions_users_unique ON coalitions_users (coalition_id, user_id);

-- ════════════════════════════════════════════════════════════════
-- SCHEMA SUMMARY
-- ════════════════════════════════════════════════════════════════
-- Metadata (reference): achievements, campuses, cursus, projects, project_sessions
-- User data: users
-- Live tracking: project_users (enrollments), achievements_users (earned), coalitions_users (membership)
-- Linkers: campus_projects, campus_achievements, coalitions
--
-- Key for live sync:
--   UPDATE users SET wallet, correction_point, location, active, updated_at
--   UPSERT project_users (user enrollment + mark/status)
--   UPSERT achievements_users (earned badges)
--   UPSERT coalitions_users (membership + score/rank)
