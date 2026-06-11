import express from "express";
import { createHash, randomBytes, scrypt as scryptCallback, timingSafeEqual } from "crypto";
import { execFile } from "child_process";
import fs from "fs";
import os from "os";
import path from "path";
import { promisify } from "util";
import { fileURLToPath } from "url";
import pg from "pg";
import { WebSocketServer } from "ws";

const { Pool } = pg;
const execFileAsync = promisify(execFile);
const scryptAsync = promisify(scryptCallback);

const {
  PORT = 3000,
  API_ROOT = "https://api.intra.42.fr",
  DB_HOST = "db",
  DB_PORT = 5432,
  DB_NAME = "api42",
  DB_USER = "api42",
  DB_PASSWORD,
  USER_DATA_STALE_AFTER_S = "600",
  AUTO_MAINTENANCE_INTERVAL_S = "3600",
  HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX = "200",
  HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX = "1000",
  HEALTH_DEGRADED_PROCESS_QUEUE_MAX = "200",
  EVENTS_WINDOW_S = "3600",
  RUNTIME_DIR = "",
  AUTH_SESSION_TTL_S = "7200",
  AUTH_COOKIE_SECURE = "",
} = process.env;

if (!DB_PASSWORD) {
  throw new Error("DB_PASSWORD environment variable is required");
}

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootDir = path.resolve(__dirname, "..");
const runtimeDir = RUNTIME_DIR || path.resolve(rootDir, "..", "runtime");
const oauthStatePath = path.join(rootDir, ".oauth_state");
const exportsRoot = path.join(runtimeDir, "exports");
const eventsQueuePath = path.join(runtimeDir, "backlog", "events_queue.jsonl");
const eventsQueueMutexPath = path.join(runtimeDir, "backlog", "events_queue.mutex");
const logsRoot = path.join(runtimeDir, "logs");
const backlogRoot = path.join(runtimeDir, "backlog");
const backupMetaPath = path.join(logsRoot, "state", "backup_latest.json");

function pickStaticDir(candidates) {
  for (const dir of candidates) {
    try {
      if (fs.existsSync(dir) && fs.statSync(dir).isDirectory()) {
        return dir;
      }
    } catch {
      // ignore candidate errors
    }
  }
  return null;
}

const staticPublicDir = pickStaticDir([
  process.env.API_STATIC_DIR || "",
  path.join(rootDir, "api", "public"),
  path.join(rootDir, "nginx", "www"),
]);

const dbPool = new Pool({
  host: DB_HOST,
  port: Number(DB_PORT),
  database: DB_NAME,
  user: DB_USER,
  password: DB_PASSWORD,
});

const STALE_AFTER_SECONDS = (() => {
  const parsed = Number(USER_DATA_STALE_AFTER_S);
  if (!Number.isFinite(parsed) || parsed < 0) {
    return 600;
  }
  return Math.floor(parsed);
})();

const EVENTS_WINDOW_SECONDS = (() => {
  const parsed = Number(EVENTS_WINDOW_S);
  if (!Number.isFinite(parsed) || parsed <= 0) {
    return 3600;
  }
  return Math.floor(parsed);
})();

const DEFAULT_QUEUE_THRESHOLDS = {
  fetch_queue_internal_warn_max: (() => {
    const parsed = Number(HEALTH_WARN_FETCH_QUEUE_INTERNAL_MAX);
    if (!Number.isFinite(parsed) || parsed < 0) return 200;
    return Math.floor(parsed);
  })(),
  fetch_queue_external_degraded_max: (() => {
    const parsed = Number(HEALTH_DEGRADED_FETCH_QUEUE_EXTERNAL_MAX);
    if (!Number.isFinite(parsed) || parsed < 0) return 1000;
    return Math.floor(parsed);
  })(),
  process_queue_degraded_max: (() => {
    const parsed = Number(HEALTH_DEGRADED_PROCESS_QUEUE_MAX);
    if (!Number.isFinite(parsed) || parsed < 0) return 200;
    return Math.floor(parsed);
  })(),
};

const EVENTS_FUTURE_GRACE_MS = 60_000;
const BACKUP_WARN_GRACE_SECONDS = 10 * 60;
const BACKUP_WARN_AGE_SECONDS = (() => {
  const parsed = Number(AUTO_MAINTENANCE_INTERVAL_S);
  const intervalSeconds = Number.isFinite(parsed) && parsed > 0 ? Math.floor(parsed) : 3600;
  return intervalSeconds + BACKUP_WARN_GRACE_SECONDS;
})();
const WS_EVENTS_SCAN_MS = 1000;
const WS_HEARTBEAT_BROADCAST_MS = 15_000;
const WS_PING_INTERVAL_MS = 30_000;
const DB_EVENTS_DEFAULT_PAGE_SIZE = 50;
const DB_EVENTS_MAX_PAGE_SIZE = 200;
const DB_EVENTS_MAX_Q_LENGTH = 120;
const PASSWORD_MIN_LENGTH = 8;
const AUTH_COOKIE_NAME = "session";
const AUTH_SCRYPT_KEYLEN = 64;
const AUTH_SCRYPT_PARAMS = {
  N: 16384,
  r: 8,
  p: 1,
};
const AUTH_SESSION_TTL_SECONDS = (() => {
  const parsed = Number(AUTH_SESSION_TTL_S);
  if (!Number.isFinite(parsed) || parsed <= 0) return 7200;
  return Math.floor(parsed);
})();
const AUTH_COOKIE_SECURE_OVERRIDE = parseOptionalBool(AUTH_COOKIE_SECURE);
const AUTH_RATE_LIMIT_WINDOW_MS = 15 * 60 * 1000;
const AUTH_RATE_LIMIT_MAX = 20;
const authAttempts = new Map();
const DB_EVENTS_SORT_COLUMNS = {
  id: "id",
  event_time: "COALESCE(event_at, updated_at, ingested_at)",
  event_type: "COALESCE(event_types->>0, '')",
  event_at: "event_at",
  updated_at: "updated_at",
  ingested_at: "ingested_at",
  user_id: "user_id",
  user_login: "user_login",
  campus_id: "campus_id",
  source: "source",
  ts: "ts",
};

const ADMIN_MICROSERVICE_CONTAINERS = [
  { key: "db", container: "transcendence_db" },
  { key: "api", container: "transcendence_api" },
  { key: "web", container: "transcendence_web" },
  { key: "detector", container: "transcendence_detector" },
  { key: "fetcher_internal", container: "transcendence_fetcher_internal" },
  { key: "fetcher_external_1", container: "transcendence_fetcher_external_1" },
  { key: "fetcher_external_2", container: "transcendence_fetcher_external_2" },
  { key: "fetcher_external_3", container: "transcendence_fetcher_external_3" },
  { key: "upserter_users", container: "transcendence_upserter_users" },
  { key: "upserter_events", container: "transcendence_upserter_events" },
  { key: "maintenance_scheduler", container: "transcendence_maintenance_scheduler" },
];

const ADMIN_API42_HEALTH_TIMEOUT_MS = (() => {
  const parsed = Number(process.env.ADMIN_API42_HEALTH_TIMEOUT_MS || "2500");
  if (!Number.isFinite(parsed) || parsed <= 0) return 2500;
  return Math.floor(parsed);
})();

const ADMIN_API42_HEALTH_CACHE_MS = (() => {
  const parsed = Number(process.env.ADMIN_API42_HEALTH_CACHE_S || "60");
  if (!Number.isFinite(parsed) || parsed < 0) return 60_000;
  return Math.floor(parsed * 1000);
})();

let adminApi42HealthCache = {
  status: null,
  updatedMs: 0,
};
let adminApi42HealthInflight = null;

function parseIntInRange(raw, { fallback, min, max }) {
  const value = Number(raw);
  if (!Number.isFinite(value)) return fallback;
  const integer = Math.trunc(value);
  if (integer < min) return min;
  if (integer > max) return max;
  return integer;
}

function parseThresholdInt(raw, fallback) {
  const value = Number(raw);
  if (!Number.isFinite(value)) return fallback;
  const integer = Math.trunc(value);
  if (integer < 0) return fallback;
  return integer;
}

function parseOptionalInt(raw, { min = Number.MIN_SAFE_INTEGER, max = Number.MAX_SAFE_INTEGER } = {}) {
  if (raw === undefined || raw === null || raw === "") return null;
  const value = Number(raw);
  if (!Number.isFinite(value)) return null;
  const integer = Math.trunc(value);
  if (integer < min || integer > max) return null;
  return integer;
}

function parseOptionalDate(raw) {
  if (!raw) return null;
  const value = String(raw).trim();
  if (!value) return null;
  const ms = Date.parse(value);
  if (!Number.isFinite(ms)) return null;
  return new Date(ms).toISOString();
}

function parseOptionalBool(raw) {
  if (raw === undefined || raw === null || raw === "") return null;
  const value = String(raw).trim().toLowerCase();
  if (["1", "true", "yes", "y"].includes(value)) return true;
  if (["0", "false", "no", "n"].includes(value)) return false;
  return null;
}

function parseSortColumn(raw) {
  const value = String(raw || "").trim().toLowerCase();
  if (value && Object.prototype.hasOwnProperty.call(DB_EVENTS_SORT_COLUMNS, value)) {
    return value;
  }
  return "event_time";
}

function parseSortDirection(raw) {
  const value = String(raw || "").trim().toLowerCase();
  return value === "asc" ? "ASC" : "DESC";
}

let authSchemaReady = null;

function ensureAuthSchema() {
  if (!authSchemaReady) {
    authSchemaReady = (async () => {
      await dbPool.query(`
        CREATE TABLE IF NOT EXISTS app_users (
          id BIGSERIAL PRIMARY KEY,
          email TEXT NOT NULL UNIQUE,
          password_hash TEXT NOT NULL,
          display_name TEXT,
          role TEXT NOT NULL DEFAULT 'user',
          created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
        )
      `);
      await dbPool.query("CREATE INDEX IF NOT EXISTS idx_app_users_email ON app_users (email)");
      await dbPool.query(`
        CREATE TABLE IF NOT EXISTS app_sessions (
          id BIGSERIAL PRIMARY KEY,
          user_id BIGINT NOT NULL REFERENCES app_users(id) ON DELETE CASCADE,
          token_hash CHAR(64) NOT NULL UNIQUE,
          created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
          expires_at TIMESTAMPTZ NOT NULL
        )
      `);
      await dbPool.query("CREATE INDEX IF NOT EXISTS idx_app_sessions_user_id ON app_sessions (user_id)");
      await dbPool.query("CREATE INDEX IF NOT EXISTS idx_app_sessions_expires_at ON app_sessions (expires_at)");
    })().catch((err) => {
      authSchemaReady = null;
      throw err;
    });
  }
  return authSchemaReady;
}

function normalizeEmail(raw) {
  return String(raw || "").trim().toLowerCase();
}

function isValidEmail(email) {
  return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email) && email.length <= 254;
}

function publicUser(row) {
  if (!row) return null;
  return {
    id: row.id,
    email: row.email,
    display_name: row.display_name,
    role: row.role,
    created_at: row.created_at,
  };
}

async function hashPassword(password) {
  const salt = randomBytes(16).toString("base64url");
  const derived = await scryptAsync(password, salt, AUTH_SCRYPT_KEYLEN, AUTH_SCRYPT_PARAMS);
  return [
    "scrypt",
    AUTH_SCRYPT_PARAMS.N,
    AUTH_SCRYPT_PARAMS.r,
    AUTH_SCRYPT_PARAMS.p,
    salt,
    Buffer.from(derived).toString("base64url"),
  ].join("$");
}

async function verifyPassword(password, storedHash) {
  const parts = String(storedHash || "").split("$");
  if (parts.length !== 6 || parts[0] !== "scrypt") return false;

  const [, nRaw, rRaw, pRaw, salt, expectedRaw] = parts;
  const params = {
    N: Number(nRaw),
    r: Number(rRaw),
    p: Number(pRaw),
  };
  if (!Number.isFinite(params.N) || !Number.isFinite(params.r) || !Number.isFinite(params.p)) {
    return false;
  }

  const expected = Buffer.from(expectedRaw, "base64url");
  if (expected.length <= 0) return false;
  const derived = await scryptAsync(password, salt, expected.length, params);
  const actual = Buffer.from(derived);
  if (actual.length !== expected.length) return false;
  return timingSafeEqual(actual, expected);
}

function parseCookies(header) {
  const cookies = {};
  for (const part of String(header || "").split(";")) {
    const eq = part.indexOf("=");
    if (eq === -1) continue;
    const key = part.slice(0, eq).trim();
    if (!key) continue;
    try {
      cookies[key] = decodeURIComponent(part.slice(eq + 1).trim());
    } catch {
      cookies[key] = part.slice(eq + 1).trim();
    }
  }
  return cookies;
}

function sessionTokenHash(token) {
  return createHash("sha256").update(String(token || "")).digest("hex");
}

function isSecureCookieRequest(req) {
  if (AUTH_COOKIE_SECURE_OVERRIDE !== null) return AUTH_COOKIE_SECURE_OVERRIDE;
  return req.secure || String(req.get("x-forwarded-proto") || "").split(",")[0].trim() === "https";
}

function setSessionCookie(req, res, token) {
  const pieces = [
    `${AUTH_COOKIE_NAME}=${encodeURIComponent(token)}`,
    "HttpOnly",
    "SameSite=Lax",
    "Path=/",
    `Max-Age=${AUTH_SESSION_TTL_SECONDS}`,
  ];
  if (isSecureCookieRequest(req)) pieces.push("Secure");
  res.setHeader("Set-Cookie", pieces.join("; "));
}

function clearSessionCookie(res) {
  res.setHeader(
    "Set-Cookie",
    `${AUTH_COOKIE_NAME}=; HttpOnly; SameSite=Lax; Path=/; Max-Age=0`
  );
}

function authRateLimitKey(req) {
  return String(req.ip || req.socket?.remoteAddress || "unknown");
}

function checkAuthRateLimit(req) {
  const key = authRateLimitKey(req);
  const now = Date.now();
  const recent = (authAttempts.get(key) || []).filter((ts) => now - ts < AUTH_RATE_LIMIT_WINDOW_MS);
  recent.push(now);
  authAttempts.set(key, recent);
  return recent.length <= AUTH_RATE_LIMIT_MAX;
}

async function createSession(req, res, userId) {
  await ensureAuthSchema();
  const token = randomBytes(32).toString("base64url");
  const tokenHash = sessionTokenHash(token);
  await dbPool.query("DELETE FROM app_sessions WHERE expires_at <= NOW()");
  await dbPool.query(
    "INSERT INTO app_sessions (user_id, token_hash, expires_at) VALUES ($1, $2, NOW() + ($3::int * INTERVAL '1 second'))",
    [userId, tokenHash, AUTH_SESSION_TTL_SECONDS]
  );
  setSessionCookie(req, res, token);
}

async function getSessionUser(req) {
  await ensureAuthSchema();
  const token = parseCookies(req.headers.cookie)[AUTH_COOKIE_NAME];
  if (!token) return null;

  const result = await dbPool.query(
    `SELECT u.id, u.email, u.display_name, u.role, u.created_at
       FROM app_sessions s
       JOIN app_users u ON u.id = s.user_id
      WHERE s.token_hash = $1
        AND s.expires_at > NOW()
      LIMIT 1`,
    [sessionTokenHash(token)]
  );
  return result.rows[0] || null;
}

async function requireAuth(req, res, next) {
  try {
    const user = await getSessionUser(req);
    if (!user) {
      return res.status(401).json({ error: "not_authenticated" });
    }
    req.authUser = user;
    return next();
  } catch (err) {
    console.error("[Auth] Session check failed:", err.message);
    return res.status(500).json({ error: "auth_check_failed" });
  }
}

function requireAdmin(req, res, next) {
  if (req.authUser?.role !== "admin") {
    return res.status(403).json({ error: "admin_required" });
  }
  return next();
}

function inferHealthFromDockerStatus(status, running) {
  const s = String(status || "").toLowerCase();
  if (!running) return "down";
  if (s.includes("(healthy)")) return "healthy";
  if (s.includes("unhealthy")) return "unhealthy";
  if (s.includes("health: starting") || s.includes("(starting)")) return "starting";
  return "running";
}

function parseDockerPsJsonLines(stdout) {
  return String(stdout || "")
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean)
    .map((line) => {
      try {
        return JSON.parse(line);
      } catch {
        return null;
      }
    })
    .filter(Boolean);
}

async function fetchLiveMicroservicesFromDocker() {
  try {
    const { stdout } = await execFileAsync(
      "docker",
      ["ps", "-a", "--format", "{{json .}}"],
      {
        timeout: 2500,
        maxBuffer: 1024 * 1024,
      }
    );

    const rows = parseDockerPsJsonLines(stdout);
    if (!rows.length) return null;

    const byName = new Map();
    for (const row of rows) {
      const name = String(row?.Names || "").trim();
      if (!name) continue;
      byName.set(name, row);
    }

    return ADMIN_MICROSERVICE_CONTAINERS.map(({ key, container }) => {
      const row = byName.get(container);
      if (!row) {
        return {
          key,
          container,
          running: false,
          health: "down",
          status: "not_running",
          ports: "-",
        };
      }

      const status = String(row.Status || "").trim();
      const running = status.toLowerCase().startsWith("up");
      return {
        key,
        container,
        running,
        health: inferHealthFromDockerStatus(status, running),
        status: status || (running ? "running" : "not_running"),
        ports: String(row.Ports || "").trim() || "no ports",
      };
    });
  } catch {
    return null;
  }
}

function defaultDownMicroservices() {
  return ADMIN_MICROSERVICE_CONTAINERS.map(({ key, container }) => ({
    key,
    container,
    running: false,
    health: "down",
    status: "not_running",
    ports: "-",
  }));
}

function parseApi42Code(statusText) {
  const match = String(statusText || "").match(/\b(\d{3})\b/);
  if (!match) return null;
  const code = Number(match[1]);
  return Number.isFinite(code) ? code : null;
}

function readDiskUsageLive(targetPath = "/") {
  try {
    const stat = fs.statfsSync(targetPath);
    const blockSize = Number(stat.bsize);
    const blocks = Number(stat.blocks);
    const blocksFree = Number(stat.bfree);
    if (
      !Number.isFinite(blockSize) ||
      !Number.isFinite(blocks) ||
      !Number.isFinite(blocksFree) ||
      blockSize <= 0 ||
      blocks <= 0
    ) {
      return null;
    }

    const totalBytes = blockSize * blocks;
    const freeBytes = Math.max(0, blockSize * blocksFree);
    const usedBytes = Math.max(0, totalBytes - freeBytes);
    const usedPercent = totalBytes > 0 ? (usedBytes / totalBytes) * 100 : null;
    const freeMb = Math.floor(freeBytes / (1024 * 1024));

    return {
      used_percent: Number.isFinite(usedPercent) ? usedPercent : null,
      free_mb: Number.isFinite(freeMb) ? freeMb : null,
    };
  } catch {
    return null;
  }
}

async function probeApi42HealthLive() {
  const oauthState = readKeyValueFile(oauthStatePath);
  const accessToken = oauthState.ACCESS_TOKEN || "";
  if (!accessToken) {
    return "missing_token 401 0.000000";
  }

  const startedMs = Date.now();
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), ADMIN_API42_HEALTH_TIMEOUT_MS);
  try {
    const response = await fetch(`${API_ROOT}/v2/me`, {
      headers: {
        Authorization: `Bearer ${accessToken}`,
        Accept: "application/json",
      },
      signal: controller.signal,
    });
    const latencyS = Math.max(0, (Date.now() - startedMs) / 1000);
    const state =
      response.status === 200
        ? "authenticated"
        : response.status === 401
        ? "unauthorized"
        : "failed";
    return `${state} ${response.status} ${latencyS.toFixed(6)}`;
  } catch {
    const latencyS = Math.max(0, (Date.now() - startedMs) / 1000);
    return `failed 503 ${latencyS.toFixed(6)}`;
  } finally {
    clearTimeout(timeout);
  }
}

async function getApi42HealthStatus() {
  const nowMs = Date.now();
  if (
    ADMIN_API42_HEALTH_CACHE_MS > 0 &&
    typeof adminApi42HealthCache.status === "string" &&
    adminApi42HealthCache.status.length > 0 &&
    nowMs - adminApi42HealthCache.updatedMs < ADMIN_API42_HEALTH_CACHE_MS
  ) {
    return adminApi42HealthCache.status;
  }

  if (adminApi42HealthInflight) {
    return adminApi42HealthInflight;
  }

  adminApi42HealthInflight = (async () => {
    const status = await probeApi42HealthLive();
    adminApi42HealthCache = {
      status,
      updatedMs: Date.now(),
    };
    return status;
  })();

  try {
    return await adminApi42HealthInflight;
  } finally {
    adminApi42HealthInflight = null;
  }
}

function classifyPulseIntensity(pulses) {
  const count = Number(pulses || 0);
  if (count >= 2000) {
    return { tier: "very_high", label: "2000+ very high" };
  }
  if (count >= 1500) {
    return { tier: "very_high", label: "1500+ very high" };
  }
  if (count >= 1000) {
    return { tier: "high", label: "1000+ high" };
  }
  if (count >= 500) {
    return { tier: "medium", label: "500+ medium" };
  }
  return { tier: "low", label: "<500 low" };
}

function readKeyValueFile(filePath) {
  try {
    const text = fs.readFileSync(filePath, "utf8");
    const values = {};
    for (const rawLine of text.split(/\r?\n/)) {
      const line = rawLine.trim();
      if (!line || line.startsWith("#")) continue;
      const eq = line.indexOf("=");
      if (eq === -1) continue;
      const key = line.slice(0, eq).trim();
      if (!key) continue;
      let value = line.slice(eq + 1).trim();
      if (
        value.length >= 2 &&
        ((value.startsWith('"') && value.endsWith('"')) ||
          (value.startsWith("'") && value.endsWith("'")))
      ) {
        value = value.slice(1, -1);
      }
      values[key] = value;
    }
    return values;
  } catch {
    return {};
  }
}

function readJsonFileIfExists(filePath) {
  if (!fs.existsSync(filePath)) return null;
  try {
    return JSON.parse(fs.readFileSync(filePath, "utf8"));
  } catch {
    return null;
  }
}

function parseQueueRecordLine(rawLine, defaultSource) {
  const line = String(rawLine || "").trim();
  if (!line) return null;
  const parts = line.split("|");
  if (parts.length !== 3) return null;

  const userId = String(parts[0] || "").trim();
  const enqueuedAtUtc = String(parts[1] || "").trim();
  const source = String(parts[2] || "").trim() || defaultSource;

  if (!/^\d+$/.test(userId)) return null;
  const enqueuedMs = Date.parse(enqueuedAtUtc);
  if (!Number.isFinite(enqueuedMs)) return null;
  if (!source) return null;

  return {
    user_id: userId,
    enqueued_at_utc: enqueuedAtUtc,
    enqueued_ms: enqueuedMs,
    source,
  };
}

function readQueueLiveStats(filePath, defaultSource) {
  if (!fs.existsSync(filePath)) {
    return {
      count: 0,
      oldest_sla_seconds: null,
      oldest_sla_minutes: null,
      oldest_enqueued_at_utc: null,
    };
  }

  let text = "";
  try {
    text = fs.readFileSync(filePath, "utf8");
  } catch {
    return {
      count: 0,
      oldest_sla_seconds: null,
      oldest_sla_minutes: null,
      oldest_enqueued_at_utc: null,
    };
  }

  const nowMs = Date.now();
  const records = text
    .split(/\r?\n/)
    .map((line) => parseQueueRecordLine(line, defaultSource))
    .filter(Boolean);

  if (!records.length) {
    return {
      count: 0,
      oldest_sla_seconds: null,
      oldest_sla_minutes: null,
      oldest_enqueued_at_utc: null,
    };
  }

  let oldestSeconds = -1;
  let oldestEnqueuedAtUtc = null;
  for (const record of records) {
    const ageSeconds = Math.max(0, Math.floor((nowMs - record.enqueued_ms) / 1000));
    if (ageSeconds > oldestSeconds) {
      oldestSeconds = ageSeconds;
      oldestEnqueuedAtUtc = record.enqueued_at_utc;
    }
  }

  return {
    count: records.length,
    oldest_sla_seconds: oldestSeconds >= 0 ? oldestSeconds : null,
    oldest_sla_minutes: oldestSeconds >= 0 ? Math.floor((oldestSeconds + 59) / 60) : null,
    oldest_enqueued_at_utc: oldestEnqueuedAtUtc,
  };
}

function asArray(value) {
  if (Array.isArray(value)) return value;
  if (value === null || value === undefined) return [];
  return [value];
}

function toEpochMs(value) {
  if (value === null || value === undefined) return null;
  if (value instanceof Date) {
    const ms = value.getTime();
    return Number.isFinite(ms) ? ms : null;
  }
  const ms = Date.parse(String(value));
  return Number.isFinite(ms) ? ms : null;
}

function computeFreshness(payload, staleAfterSeconds) {
  const candidates = [
    payload?.user?.updated_at,
    payload?.user?.ingested_at,
    payload?.user?.created_at,
    payload?._meta?.file_mtime_ms,
  ]
    .map(toEpochMs)
    .filter((v) => v !== null);

  if (candidates.length === 0) {
    return {
      freshest_ms: null,
      age_seconds: null,
      stale_after_s: staleAfterSeconds,
      stale: false,
    };
  }

  const freshestMs = Math.max(...candidates);
  const ageSeconds = Math.max(0, Math.floor((Date.now() - freshestMs) / 1000));

  return {
    freshest_ms: freshestMs,
    age_seconds: ageSeconds,
    stale_after_s: staleAfterSeconds,
    stale: ageSeconds > staleAfterSeconds,
  };
}

function readEventsAllRecords() {
  if (!fs.existsSync(eventsQueuePath)) return [];
  const text = fs.readFileSync(eventsQueuePath, "utf8");
  return text
    .split("\n")
    .filter((line) => line.trim().length > 0)
    .map((line) => {
      try {
        return { event: JSON.parse(line), line };
      } catch {
        return null;
      }
    })
    .filter(Boolean);
}

function eventRecordIdFromLine(line) {
  return createHash("sha1").update(String(line || "")).digest("hex").slice(0, 24);
}

function eventTimeMs(event) {
  if (!event || typeof event !== "object") return null;

  if (typeof event.ts === "number" && Number.isFinite(event.ts)) {
    return event.ts * 1000;
  }

  if (typeof event.updated_at === "string") {
    const ms = Date.parse(event.updated_at);
    if (Number.isFinite(ms)) return ms;
  }

  return null;
}

function sortEnvelopesAscending(envelopes) {
  return envelopes.sort((a, b) => {
    if (a.event_ms !== b.event_ms) return a.event_ms - b.event_ms;
    return String(a.id).localeCompare(String(b.id));
  });
}

function toPublicWsEnvelope(envelope) {
  return {
    id: envelope.id,
    event_ms: envelope.event_ms,
    event: envelope.event,
  };
}

async function readWindowEventEnvelopes({ pruneQueue = true } = {}) {
  return await withExclusiveFileLock(eventsQueueMutexPath, async () => {
    const records = readEventsAllRecords();
    const nowMs = Date.now();
    const cutoffMs = nowMs - EVENTS_WINDOW_SECONDS * 1000;

    const windowRecords = records.filter((record) => {
      const ms = eventTimeMs(record.event);
      if (!Number.isFinite(ms)) return false;
      if (ms < cutoffMs) return false;
      if (ms > nowMs + EVENTS_FUTURE_GRACE_MS) return false;
      return true;
    });

    if (pruneQueue) {
      // Keep only the rolling window in the queue file so it cannot grow unbounded.
      if (windowRecords.length > 0) {
        const text = `${windowRecords.map((r) => r.line).join("\n")}\n`;
        fs.writeFileSync(eventsQueuePath, text, "utf8");
      } else {
        fs.writeFileSync(eventsQueuePath, "", "utf8");
      }
    }

    const envelopes = sortEnvelopesAscending(
      windowRecords.map((record) => ({
        id: eventRecordIdFromLine(record.line),
        event_ms: eventTimeMs(record.event),
        event: record.event,
      }))
    );

    return {
      now_ms: nowMs,
      timestamp_utc: new Date(nowMs).toISOString(),
      envelopes,
    };
  });
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function withExclusiveFileLock(
  lockPath,
  fn,
  { timeoutMs = 3000, retryDelayMs = 25 } = {}
) {
  await fs.promises.mkdir(path.dirname(lockPath), { recursive: true });

  const deadline = Date.now() + timeoutMs;
  let handle = null;

  while (handle === null) {
    try {
      handle = await fs.promises.open(lockPath, "wx", 0o644);
    } catch (err) {
      if (err && err.code === "EEXIST") {
        if (Date.now() >= deadline) {
          throw new Error(`timeout acquiring events lock: ${lockPath}`);
        }
        await sleep(retryDelayMs);
        continue;
      }

      if (err && err.code === "ENOENT") {
        await fs.promises.mkdir(path.dirname(lockPath), { recursive: true });
        continue;
      }

      throw err;
    }
  }

  try {
    return await fn();
  } finally {
    try {
      await handle.close();
    } catch {
      // ignore lock close errors
    }
    try {
      await fs.promises.unlink(lockPath);
    } catch {
      // ignore lock unlink errors
    }
  }
}

async function fetchUserFromDb(userId) {
  const userResult = await dbPool.query("SELECT * FROM users WHERE id = $1 LIMIT 1", [userId]);
  if (userResult.rows.length === 0) {
    return null;
  }

  const [projectsResult, achievementsResult, coalitionsResult] = await Promise.all([
    dbPool
      .query(
        "SELECT * FROM project_users WHERE user_id = $1 ORDER BY updated_at DESC NULLS LAST LIMIT 200",
        [userId]
      )
      .catch(() => ({ rows: [] })),
    dbPool
      .query(
        "SELECT * FROM achievements_users WHERE user_id = $1 ORDER BY updated_at DESC NULLS LAST LIMIT 200",
        [userId]
      )
      .catch(() => ({ rows: [] })),
    dbPool
      .query(
        "SELECT * FROM coalitions_users WHERE user_id = $1 ORDER BY updated_at DESC NULLS LAST LIMIT 200",
        [userId]
      )
      .catch(() => ({ rows: [] })),
  ]);

  return {
    user: userResult.rows[0],
    projects_users: projectsResult.rows,
    achievements_users: achievementsResult.rows,
    coalitions_users: coalitionsResult.rows,
  };
}

function fetchUserFromExports(userId) {
  const usersDir = path.join(exportsRoot, "09_users");
  if (!fs.existsSync(usersDir)) {
    return null;
  }

  const filename = `user_${userId}.json`;
  let campusBucket = null;
  let user = null;
  let userFilePath = null;

  for (const entry of fs.readdirSync(usersDir, { withFileTypes: true })) {
    if (!entry.isDirectory() || !entry.name.startsWith("campus_")) continue;
    const candidate = path.join(usersDir, entry.name, filename);
    const parsed = readJsonFileIfExists(candidate);
    if (parsed !== null) {
      campusBucket = entry.name;
      user = parsed;
      userFilePath = candidate;
      break;
    }
  }

  if (user === null) {
    return null;
  }

  // Legacy per-user exports 10/11/12 were removed. Use nested arrays from 09_users payload.
  const projects = asArray(user.projects_users);
  const achievements = asArray(user.achievements);
  const coalitions = asArray(user.coalitions_users);

  let fileMtimeMs = null;
  if (userFilePath) {
    try {
      fileMtimeMs = fs.statSync(userFilePath).mtimeMs;
    } catch {
      fileMtimeMs = null;
    }
  }

  return {
    user,
    projects_users: projects,
    achievements_users: achievements,
    coalitions_users: coalitions,
    campus_bucket: campusBucket,
    _meta: {
      file_mtime_ms: fileMtimeMs,
    },
  };
}

async function fetchUserDirectFrom42(userId) {
  const oauthState = readKeyValueFile(oauthStatePath);
  const accessToken = oauthState.ACCESS_TOKEN || "";
  if (!accessToken) {
    throw new Error("missing ACCESS_TOKEN in .oauth_state");
  }

  const response = await fetch(`${API_ROOT}/v2/users/${userId}`, {
    headers: {
      Authorization: `Bearer ${accessToken}`,
      Accept: "application/json",
    },
  });

  if (response.status === 404) {
    return null;
  }

  if (!response.ok) {
    const details = await response.text();
    throw new Error(`42 API error ${response.status}: ${details.slice(0, 200)}`);
  }

  return await response.json();
}

const app = express();
app.set("trust proxy", true);
app.use(express.json({ limit: "2mb" }));

if (staticPublicDir) {
  app.use(
    express.static(staticPublicDir, {
      dotfiles: "deny",
      index: false,
      fallthrough: true,
    })
  );
}

app.get("/", (_req, res) => {
  const staticLoginPath = staticPublicDir ? path.join(staticPublicDir, "login.html") : null;
  if (staticLoginPath && fs.existsSync(staticLoginPath)) {
    return res.sendFile(staticLoginPath);
  }
  return res.json({ service: "api", status: "ok" });
});

app.get("/health", (_req, res) => {
  res.json({ status: "ok" });
});

app.post("/api/auth/signup", async (req, res) => {
  if (!checkAuthRateLimit(req)) {
    return res.status(429).json({ error: "too_many_auth_attempts" });
  }

  const email = normalizeEmail(req.body?.email);
  const password = String(req.body?.password || "");
  const displayName = String(req.body?.display_name || "").trim().slice(0, 120) || null;

  if (!isValidEmail(email)) {
    return res.status(400).json({ error: "valid_email_required" });
  }
  if (password.length < PASSWORD_MIN_LENGTH) {
    return res.status(400).json({ error: "password_too_short", min_length: PASSWORD_MIN_LENGTH });
  }

  try {
    await ensureAuthSchema();
    const passwordHash = await hashPassword(password);
    const countResult = await dbPool.query("SELECT COUNT(*)::int AS count FROM app_users");
    const role = Number(countResult.rows[0]?.count || 0) === 0 ? "admin" : "user";
    const result = await dbPool.query(
      `INSERT INTO app_users (email, password_hash, display_name, role)
       VALUES ($1, $2, $3, $4)
       RETURNING id, email, display_name, role, created_at`,
      [email, passwordHash, displayName, role]
    );
    await createSession(req, res, result.rows[0].id);
    return res.status(201).json({ user: publicUser(result.rows[0]) });
  } catch (err) {
    if (err?.code === "23505") {
      return res.status(409).json({ error: "email_already_registered" });
    }
    console.error("[Auth] Signup failed:", err.message);
    return res.status(500).json({ error: "signup_failed" });
  }
});

app.post("/api/auth/login", async (req, res) => {
  if (!checkAuthRateLimit(req)) {
    return res.status(429).json({ error: "too_many_auth_attempts" });
  }

  const email = normalizeEmail(req.body?.email);
  const password = String(req.body?.password || "");

  if (!isValidEmail(email) || !password) {
    return res.status(401).json({ error: "invalid_email_or_password" });
  }

  try {
    await ensureAuthSchema();
    const result = await dbPool.query(
      "SELECT id, email, password_hash, display_name, role, created_at FROM app_users WHERE email = $1 LIMIT 1",
      [email]
    );
    const user = result.rows[0];
    if (!user || !(await verifyPassword(password, user.password_hash))) {
      return res.status(401).json({ error: "invalid_email_or_password" });
    }
    await createSession(req, res, user.id);
    return res.json({ user: publicUser(user) });
  } catch (err) {
    console.error("[Auth] Login failed:", err.message);
    return res.status(500).json({ error: "login_failed" });
  }
});

app.post("/api/auth/logout", async (req, res) => {
  try {
    await ensureAuthSchema();
    const token = parseCookies(req.headers.cookie)[AUTH_COOKIE_NAME];
    if (token) {
      await dbPool.query("DELETE FROM app_sessions WHERE token_hash = $1", [sessionTokenHash(token)]);
    }
    clearSessionCookie(res);
    return res.json({ ok: true });
  } catch (err) {
    console.error("[Auth] Logout failed:", err.message);
    clearSessionCookie(res);
    return res.status(500).json({ error: "logout_failed" });
  }
});

app.get("/api/auth/me", async (req, res) => {
  try {
    const user = await getSessionUser(req);
    if (!user) {
      return res.status(401).json({ error: "not_authenticated" });
    }
    return res.json({ user: publicUser(user) });
  } catch (err) {
    console.error("[Auth] Me failed:", err.message);
    return res.status(500).json({ error: "auth_me_failed" });
  }
});

app.patch("/api/auth/change-username", requireAuth, async (req, res) => {
  const oldPassword   = String(req.body?.old_password   || "");
  const newUsername   = String(req.body?.new_username   || "").trim().slice(0, 120);

  if (!oldPassword || !newUsername) {
    return res.status(400).json({ error: "all_fields_required" });
  }

  try {
    const userResult = await dbPool.query(
      "SELECT password_hash FROM app_users WHERE id = $1",
      [req.authUser.id]
    );
    const user = userResult.rows[0];
    if (!user || !(await verifyPassword(oldPassword, user.password_hash))) {
      return res.status(401).json({ error: "invalid_old_password" });
    }
    const taken = await dbPool.query(
      "SELECT id FROM app_users WHERE LOWER(display_name) = LOWER($1) AND id != $2 LIMIT 1",
      [newUsername, req.authUser.id]
    );
    if (taken.rows.length > 0) {
      return res.status(409).json({ error: "username_already_taken" });
    }
    await dbPool.query(
      "UPDATE app_users SET display_name = $1 WHERE id = $2",
      [newUsername, req.authUser.id]
    );
    return res.json({ ok: true });
  } catch (err) {
    console.error("[Auth] Change username failed:", err.message);
    return res.status(500).json({ error: "change_username_failed" });
  }
});

app.patch("/api/auth/change-password", requireAuth, async (req, res) => {
  const oldPassword = String(req.body?.old_password || "");
  const newPassword = String(req.body?.new_password || "");

  if (!oldPassword || !newPassword) {
    return res.status(400).json({ error: "all_fields_required" });
  }
  if (newPassword.length < PASSWORD_MIN_LENGTH) {
    return res.status(400).json({ error: "password_too_short", min_length: PASSWORD_MIN_LENGTH });
  }

  try {
    const result = await dbPool.query(
      "SELECT password_hash FROM app_users WHERE id = $1",
      [req.authUser.id]
    );
    const user = result.rows[0];
    if (!user || !(await verifyPassword(oldPassword, user.password_hash))) {
      return res.status(401).json({ error: "invalid_old_password" });
    }
    const newHash = await hashPassword(newPassword);
    await dbPool.query(
      "UPDATE app_users SET password_hash = $1 WHERE id = $2",
      [newHash, req.authUser.id]
    );
    return res.json({ ok: true });
  } catch (err) {
    console.error("[Auth] Change password failed:", err.message);
    return res.status(500).json({ error: "change_password_failed" });
  }
});

app.patch("/api/auth/change-email", requireAuth, async (req, res) => {
  const oldPassword = String(req.body?.old_password || "");
  const newEmail    = normalizeEmail(req.body?.new_email);

  if (!oldPassword || !newEmail) {
    return res.status(400).json({ error: "all_fields_required" });
  }
  if (!isValidEmail(newEmail)) {
    return res.status(400).json({ error: "valid_email_required" });
  }

  try {
    const result = await dbPool.query(
      "SELECT password_hash FROM app_users WHERE id = $1",
      [req.authUser.id]
    );
    const user = result.rows[0];
    if (!user || !(await verifyPassword(oldPassword, user.password_hash))) {
      return res.status(401).json({ error: "invalid_old_password" });
    }
    await dbPool.query(
      "UPDATE app_users SET email = $1 WHERE id = $2",
      [newEmail, req.authUser.id]
    );
    return res.json({ ok: true });
  } catch (err) {
    if (err?.code === "23505") {
      return res.status(409).json({ error: "email_already_registered" });
    }
    console.error("[Auth] Change email failed:", err.message);
    return res.status(500).json({ error: "change_email_failed" });
  }
});

app.get("/api/users/:id", async (req, res) => {
  const rawId = String(req.params.id || "").trim();
  if (!/^\d+$/.test(rawId)) {
    return res.status(400).json({ error: "invalid_user_id" });
  }

  const userId = Number(rawId);
  const staleAfterSeconds = STALE_AFTER_SECONDS;

  try {
    let localPayload = null;
    let localSource = null;

    try {
      const fromDb = await fetchUserFromDb(userId);
      if (fromDb) {
        localPayload = fromDb;
        localSource = "db";
      }
    } catch (dbErr) {
      console.warn(`[Users API] DB lookup failed for ${userId}: ${dbErr.message}`);
    }

    if (!localPayload) {
      const fromExports = fetchUserFromExports(userId);
      if (fromExports) {
        localPayload = fromExports;
        localSource = "exports";
      }
    }

    if (localPayload) {
      const freshness = computeFreshness(localPayload, staleAfterSeconds);
      if (freshness.stale) {
        try {
          const from42 = await fetchUserDirectFrom42(userId);
          if (from42) {
            return res.json({
              source: "api42",
              id: userId,
              user: from42,
              projects_users: [],
              achievements_users: [],
              coalitions_users: [],
              persisted: false,
              refresh_reason: "stale_local_data",
              freshness,
            });
          }
        } catch (refreshErr) {
          return res.json({
            source: localSource,
            id: userId,
            ...localPayload,
            freshness,
            refresh_error: refreshErr.message,
          });
        }
      }

      return res.json({
        source: localSource,
        id: userId,
        ...localPayload,
        freshness,
      });
    }

    const from42 = await fetchUserDirectFrom42(userId);
    if (from42) {
      return res.json({
        source: "api42",
        id: userId,
        user: from42,
        projects_users: [],
        achievements_users: [],
        coalitions_users: [],
        persisted: false,
        refresh_reason: "missing_local_data",
        freshness: {
          freshest_ms: null,
          age_seconds: null,
          stale_after_s: staleAfterSeconds,
          stale: false,
        },
      });
    }

    return res.status(404).json({ error: "user_not_found", id: userId });
  } catch (err) {
    console.error(`[Users API] Failed to resolve user ${userId}:`, err.message);
    return res.status(502).json({
      error: "user_lookup_failed",
      id: userId,
      details: err.message,
    });
  }
});

app.get("/api/events/latest", async (_req, res) => {
  try {
    const snapshot = await readWindowEventEnvelopes();
    const events = snapshot.envelopes.map((env) => env.event);

    res.setHeader("Cache-Control", "no-store");
    return res.json({
      timestamp: snapshot.timestamp_utc,
      count: events.length,
      events,
    });
  } catch (err) {
    console.error("[Events Latest] Error:", err.message);
    return res.status(500).json({ error: "events_latest_failed", details: err.message });
  }
});

app.get("/api/events/db/meta", async (_req, res) => {
  try {
    const [summaryResult, sourcesResult, eventTypesResult, campusesResult] = await Promise.all([
      dbPool.query(
        `SELECT
           COUNT(*)::bigint AS total_rows,
           MIN(COALESCE(event_at, updated_at, ingested_at)) AS min_event_time,
           MAX(COALESCE(event_at, updated_at, ingested_at)) AS max_event_time
         FROM detector_events`
      ),
      dbPool.query(
        `SELECT source, COUNT(*)::bigint AS count
         FROM detector_events
         GROUP BY source
         ORDER BY count DESC, source ASC`
      ),
      dbPool.query(
        `SELECT t.event_type, COUNT(*)::bigint AS count
         FROM detector_events de,
              LATERAL jsonb_array_elements_text(de.event_types) AS t(event_type)
         GROUP BY t.event_type
         ORDER BY count DESC, t.event_type ASC
         LIMIT 100`
      ),
      dbPool.query(
        `SELECT campus_id, COUNT(*)::bigint AS count
         FROM detector_events
         WHERE campus_id IS NOT NULL
         GROUP BY campus_id
         ORDER BY count DESC, campus_id ASC
         LIMIT 100`
      ),
    ]);

    const summary = summaryResult.rows[0] || {};

    return res.json({
      timestamp_utc: new Date().toISOString(),
      summary: {
        total_rows: Number(summary.total_rows || 0),
        min_event_time: summary.min_event_time || null,
        max_event_time: summary.max_event_time || null,
      },
      sources: sourcesResult.rows.map((row) => ({
        source: row.source,
        count: Number(row.count || 0),
      })),
      event_types: eventTypesResult.rows.map((row) => ({
        event_type: row.event_type,
        count: Number(row.count || 0),
      })),
      campuses: campusesResult.rows.map((row) => ({
        campus_id: Number(row.campus_id),
        count: Number(row.count || 0),
      })),
      sort_columns: Object.keys(DB_EVENTS_SORT_COLUMNS),
    });
  } catch (err) {
    console.error("[Events DB Meta] Error:", err.message);
    return res.status(500).json({ error: "events_db_meta_failed", details: err.message });
  }
});

app.get("/api/events/db", async (req, res) => {
  try {
    const page = parseIntInRange(req.query.page, {
      fallback: 1,
      min: 1,
      max: 1_000_000,
    });
    const pageSize = parseIntInRange(req.query.page_size, {
      fallback: DB_EVENTS_DEFAULT_PAGE_SIZE,
      min: 1,
      max: DB_EVENTS_MAX_PAGE_SIZE,
    });
    const sortByKey = parseSortColumn(req.query.sort_by);
    const sortDirection = parseSortDirection(req.query.sort_dir);
    const sortExpression = DB_EVENTS_SORT_COLUMNS[sortByKey];
    const q = String(req.query.q || "").trim().slice(0, DB_EVENTS_MAX_Q_LENGTH);
    const source = String(req.query.source || "").trim();
    const eventType = String(req.query.event_type || "").trim();
    const campusId = parseOptionalInt(req.query.campus_id, { min: 0 });
    const userId = parseOptionalInt(req.query.user_id, { min: 0 });
    const from = parseOptionalDate(req.query.from);
    const to = parseOptionalDate(req.query.to);
    const firstSnapshot = parseOptionalBool(req.query.first_snapshot);

    const where = [];
    const params = [];
    const bind = (value) => {
      params.push(value);
      return `$${params.length}`;
    };

    if (source) {
      where.push(`source = ${bind(source)}`);
    }
    if (eventType) {
      where.push(`event_types ? ${bind(eventType)}`);
    }
    if (campusId !== null) {
      where.push(`campus_id = ${bind(campusId)}`);
    }
    if (userId !== null) {
      where.push(`user_id = ${bind(userId)}`);
    }
    if (firstSnapshot !== null) {
      where.push(`first_snapshot = ${bind(firstSnapshot)}`);
    }
    if (from) {
      where.push(`COALESCE(event_at, updated_at, ingested_at) >= ${bind(from)}::timestamptz`);
    }
    if (to) {
      where.push(`COALESCE(event_at, updated_at, ingested_at) <= ${bind(to)}::timestamptz`);
    }
    if (q) {
      const likePattern = `%${q.replace(/[%_\\]/g, "\\$&")}%`;
      const pLogin = bind(likePattern);
      const pUserId = bind(likePattern);
      const pCampus = bind(likePattern);
      const pType = bind(likePattern);
      where.push(`(
        user_login ILIKE ${pLogin} ESCAPE '\\'
        OR CAST(user_id AS TEXT) ILIKE ${pUserId} ESCAPE '\\'
        OR CAST(campus_id AS TEXT) ILIKE ${pCampus} ESCAPE '\\'
        OR EXISTS (
          SELECT 1
          FROM jsonb_array_elements_text(event_types) AS t(type)
          WHERE t.type ILIKE ${pType} ESCAPE '\\'
        )
      )`);
    }

    const whereSql = where.length > 0 ? `WHERE ${where.join(" AND ")}` : "";

    const countQuery = `SELECT COUNT(*)::bigint AS total FROM detector_events ${whereSql}`;
    const countResult = await dbPool.query(countQuery, params);
    const total = Number(countResult.rows[0]?.total || 0);
    const totalPages = Math.max(1, Math.ceil(total / pageSize));
    const safePage = Math.min(page, totalPages);
    const offset = (safePage - 1) * pageSize;

    const sortSql =
      sortByKey === "id"
        ? `id ${sortDirection}`
        : `${sortExpression} ${sortDirection} NULLS LAST, id ${sortDirection}`;

    const dataQuery = `
      SELECT
        id,
        event_uid,
        source,
        ts,
        event_at,
        updated_at,
        user_id,
        user_login,
        campus_id,
        first_snapshot,
        event_types,
        changes,
        payload,
        ingested_at,
        COALESCE(event_at, updated_at, ingested_at) AS event_time
      FROM detector_events
      ${whereSql}
      ORDER BY ${sortSql}
      LIMIT $${params.length + 1}
      OFFSET $${params.length + 2}
    `;

    const dataParams = [...params, pageSize, offset];
    const dataResult = await dbPool.query(dataQuery, dataParams);

    return res.json({
      timestamp_utc: new Date().toISOString(),
      pagination: {
        page: safePage,
        page_size: pageSize,
        total,
        total_pages: totalPages,
      },
      sorting: {
        sort_by: sortByKey,
        sort_dir: sortDirection.toLowerCase(),
      },
      filters: {
        q,
        source: source || null,
        event_type: eventType || null,
        campus_id: campusId,
        user_id: userId,
        first_snapshot: firstSnapshot,
        from,
        to,
      },
      items: dataResult.rows,
    });
  } catch (err) {
    console.error("[Events DB] Error:", err.message);
    return res.status(500).json({ error: "events_db_failed", details: err.message });
  }
});

app.get("/api/events/db/dashboard", async (req, res) => {
  try {
    const hours = parseIntInRange(req.query.hours, {
      fallback: 24,
      min: 1,
      max: 168,
    });

    const [histogramResult, totalsResult, campusesResult, countriesResult] = await Promise.all([
      dbPool.query(
        `WITH frame AS (
           SELECT date_trunc('hour', timezone('UTC', now())) AS now_hour_utc
         ),
         bins AS (
           SELECT generate_series(
             (SELECT now_hour_utc FROM frame) - (($1::int - 1) * interval '1 hour'),
             (SELECT now_hour_utc FROM frame),
             interval '1 hour'
           ) AS hour_utc
         ),
         agg AS (
           SELECT
             date_trunc('hour', timezone('UTC', COALESCE(event_at, updated_at, ingested_at))) AS hour_utc,
             COUNT(*)::bigint AS pulses
           FROM detector_events
           WHERE COALESCE(event_at, updated_at, ingested_at) >= now() - make_interval(hours => $1::int)
           GROUP BY 1
         )
         SELECT
           to_char(bins.hour_utc, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS hour_utc,
           COALESCE(agg.pulses, 0)::bigint AS pulses
         FROM bins
         LEFT JOIN agg ON agg.hour_utc = bins.hour_utc
         ORDER BY bins.hour_utc ASC`,
        [hours]
      ),
      dbPool.query(
        `SELECT
           COUNT(*)::bigint AS total_pulses_db,
           COUNT(DISTINCT user_id) FILTER (WHERE user_id IS NOT NULL)::bigint AS unique_users_db,
           COUNT(*) FILTER (
             WHERE COALESCE(event_at, updated_at, ingested_at) >= now() - interval '1 hour'
           )::bigint AS pulses_last_hour,
           COUNT(DISTINCT user_id) FILTER (
             WHERE user_id IS NOT NULL
               AND COALESCE(event_at, updated_at, ingested_at) >= now() - interval '1 hour'
           )::bigint AS unique_users_last_hour,
           COUNT(*) FILTER (
             WHERE COALESCE(event_at, updated_at, ingested_at) >= now() - make_interval(hours => $1::int)
           )::bigint AS pulses_window,
           COUNT(DISTINCT user_id) FILTER (
             WHERE user_id IS NOT NULL
               AND COALESCE(event_at, updated_at, ingested_at) >= now() - make_interval(hours => $1::int)
           )::bigint AS unique_users_window
         FROM detector_events`,
        [hours]
      ),
      dbPool.query(
        `SELECT
           de.campus_id,
           c.name AS campus_name,
           c.country AS campus_country,
           COUNT(*)::bigint AS pulses,
           COUNT(DISTINCT de.user_id) FILTER (WHERE de.user_id IS NOT NULL)::bigint AS unique_users
         FROM detector_events de
         LEFT JOIN campuses c ON c.id = de.campus_id
         WHERE de.campus_id IS NOT NULL
           AND COALESCE(de.event_at, de.updated_at, de.ingested_at) >= now() - make_interval(hours => $1::int)
         GROUP BY de.campus_id, c.name, c.country
         ORDER BY pulses DESC, de.campus_id ASC`,
        [hours]
      ),
      dbPool.query(
        `SELECT
           COALESCE(c.country, 'Unknown') AS country,
           COUNT(*)::bigint AS pulses,
           COUNT(DISTINCT de.user_id) FILTER (WHERE de.user_id IS NOT NULL)::bigint AS unique_users
         FROM detector_events de
         LEFT JOIN campuses c ON c.id = de.campus_id
         WHERE de.campus_id IS NOT NULL
           AND COALESCE(de.event_at, de.updated_at, de.ingested_at) >= now() - make_interval(hours => $1::int)
         GROUP BY COALESCE(c.country, 'Unknown')
         ORDER BY pulses DESC, country ASC`,
        [hours]
      ),
    ]);

    const histogram = histogramResult.rows.map((row) => ({
      hour_utc: row.hour_utc,
      pulses: Number(row.pulses || 0),
    }));

    const totals = totalsResult.rows[0] || {};
    const currentHour = histogram.length > 0 ? histogram[histogram.length - 1] : { hour_utc: null, pulses: 0 };
    const pulsesLastHour = Number(totals.pulses_last_hour || 0);
    const lastHourPulseIntensity = classifyPulseIntensity(pulsesLastHour);
    const pulsesWindow = Number(totals.pulses_window || 0);

    const campusesByPulses = campusesResult.rows.map((row, index) => ({
      rank: index + 1,
      campus_id: Number(row.campus_id),
      campus_name: row.campus_name || null,
      campus_country: row.campus_country || null,
      pulses: Number(row.pulses || 0),
      unique_users: Number(row.unique_users || 0),
      share_pct: pulsesWindow > 0 ? Number(((Number(row.pulses || 0) / pulsesWindow) * 100).toFixed(2)) : 0,
    }));

    const countriesByPulses = countriesResult.rows.map((row, index) => ({
      rank: index + 1,
      country: row.country || "Unknown",
      pulses: Number(row.pulses || 0),
      unique_users: Number(row.unique_users || 0),
      share_pct: pulsesWindow > 0 ? Number(((Number(row.pulses || 0) / pulsesWindow) * 100).toFixed(2)) : 0,
    }));

    return res.json({
      timestamp_utc: new Date().toISOString(),
      source: "detector_events",
      window_hours: hours,
      pulse_scale: {
        low: 500,
        medium: 1000,
        high: 1500,
        very_high: 2000,
      },
      summary: {
        total_pulses_db: Number(totals.total_pulses_db || 0),
        unique_users_db: Number(totals.unique_users_db || 0),
        pulses_last_hour: pulsesLastHour,
        unique_users_last_hour: Number(totals.unique_users_last_hour || 0),
        last_hour_intensity: lastHourPulseIntensity,
        pulses_window: pulsesWindow,
        unique_users_window: Number(totals.unique_users_window || 0),
        current_hour_utc: currentHour.hour_utc,
        current_hour_pulses: Number(currentHour.pulses || 0),
        current_hour_intensity: classifyPulseIntensity(currentHour.pulses),
      },
      histogram_by_hour: histogram,
      campuses_by_pulses: campusesByPulses,
      countries_by_pulses: countriesByPulses,
    });
  } catch (err) {
    console.error("[Events DB Dashboard] Error:", err.message);
    return res.status(500).json({ error: "events_db_dashboard_failed", details: err.message });
  }
});

app.get("/api/admin/status", requireAuth, requireAdmin, async (_req, res) => {
  const backupMeta = readJsonFileIfExists(backupMetaPath) || {};
  const liveMicroservices = await fetchLiveMicroservicesFromDocker();
  const dockerAvailable = Array.isArray(liveMicroservices) && liveMicroservices.length > 0;
  const microservices = dockerAvailable ? liveMicroservices : defaultDownMicroservices();
  const microservicesSource =
    dockerAvailable ? "live_docker" : "docker_unavailable";

  const queueFiles = {
    fetch_queue_internal: {
      path: path.join(backlogRoot, "fetch_queue_internal.txt"),
      source: "internal",
    },
    fetch_queue_external: {
      path: path.join(backlogRoot, "fetch_queue_external.txt"),
      source: "external",
    },
    process_queue: {
      path: path.join(backlogRoot, "process_queue.txt"),
      source: "process",
    },
  };
  const liveQueueStats = Object.fromEntries(
    Object.entries(queueFiles).map(([queueName, queueCfg]) => [
      queueName,
      readQueueLiveStats(queueCfg.path, queueCfg.source),
    ])
  );
  const queueThresholds = {
    fetch_queue_internal_warn_max: DEFAULT_QUEUE_THRESHOLDS.fetch_queue_internal_warn_max,
    fetch_queue_external_degraded_max: DEFAULT_QUEUE_THRESHOLDS.fetch_queue_external_degraded_max,
    process_queue_degraded_max: DEFAULT_QUEUE_THRESHOLDS.process_queue_degraded_max,
  };
  const queuesStatus = Object.fromEntries(
    Object.entries(queueFiles).map(([key]) => {
      const liveStats = liveQueueStats?.[key] || {};
      return [
        key,
        {
          count: parseOptionalInt(liveStats.count, { min: 0 }) ?? 0,
          oldest_sla_seconds: parseOptionalInt(liveStats.oldest_sla_seconds, { min: 0 }),
          oldest_sla_minutes: parseOptionalInt(liveStats.oldest_sla_minutes, { min: 0 }),
        },
      ];
    })
  );

  let dbSizeBytes = null;
  try {
    const sizeResult = await dbPool.query("SELECT pg_database_size(current_database()) AS size_bytes");
    dbSizeBytes = Number(sizeResult?.rows?.[0]?.size_bytes ?? 0);
  } catch {
    dbSizeBytes = null;
  }

  const loadAvg = os.loadavg();
  const memoryTotal = os.totalmem();
  const memoryFree = os.freemem();
  const memoryUsed = Math.max(0, memoryTotal - memoryFree);
  const diskUsage = readDiskUsageLive("/");
  const api42Status = await getApi42HealthStatus();
  const api42Code = parseApi42Code(api42Status);

  const servicesSummary = {
    running: microservices.filter((svc) => svc?.running).length,
    healthy: microservices.filter((svc) => String(svc?.health || "") === "healthy").length,
    total: microservices.length,
  };
  const criticalServiceKeys = ["db", "api", "web", "detector", "upserter_events"];
  const warningServiceKeys = [
    "fetcher_internal",
    "fetcher_external_1",
    "fetcher_external_2",
    "fetcher_external_3",
    "upserter_users",
    "maintenance_scheduler",
  ];
  const serviceByKey = new Map(
    microservices
      .filter(Boolean)
      .map((svc) => [String(svc.key || ""), svc])
  );
  const serviceIsHealthy = (key) => {
    const svc = serviceByKey.get(key);
    if (!svc) return false;
    return !!svc.running && String(svc.health || "") === "healthy";
  };
  const criticalServiceUnhealthy = criticalServiceKeys.filter((key) => !serviceIsHealthy(key));
  const warningServiceUnhealthy = warningServiceKeys.filter((key) => !serviceIsHealthy(key));
  const requiredSummary =
    (() => {
      if (!microservices.length) return null;
      let running = 0;
      let healthy = 0;
      const total = criticalServiceKeys.length;
      for (const key of criticalServiceKeys) {
        const svc = serviceByKey.get(key);
        if (!svc) continue;
        if (svc.running) running += 1;
        if (String(svc.health || "") === "healthy") healthy += 1;
      }
      return { running, healthy, total, optional: warningServiceKeys };
    })();

  const qInt = Number(queuesStatus.fetch_queue_internal?.count || 0);
  const qExt = Number(queuesStatus.fetch_queue_external?.count || 0);
  const qProc = Number(queuesStatus.process_queue?.count || 0);
  const backupStatus = String(backupMeta.status || "unknown").toLowerCase();
  const api42StatusLower = String(api42Status || "").toLowerCase();
  const api42Unavailable =
    api42Code === null ||
    api42StatusLower.startsWith("missing_token") ||
    api42StatusLower.startsWith("curl_unavailable") ||
    api42StatusLower.startsWith("unavailable") ||
    api42StatusLower.startsWith("unknown") ||
    (api42Code !== null && api42Code >= 400 && api42Code < 500 && api42Code !== 429);
  const api42TransientWarn =
    api42Code === 429 ||
    (api42Code !== null && api42Code >= 500);
  const queueInternalWarn = qInt > queueThresholds.fetch_queue_internal_warn_max;
  const queueExternalWarn = qExt > queueThresholds.fetch_queue_external_degraded_max;
  const queueProcessWarn = qProc > queueThresholds.process_queue_degraded_max;
  const backupTimestampUtc = backupMeta.timestamp_utc || null;
  const backupTimestampMs = backupTimestampUtc ? Date.parse(String(backupTimestampUtc)) : NaN;
  const backupAgeSeconds = Number.isFinite(backupTimestampMs)
    ? Math.max(0, Math.floor((Date.now() - backupTimestampMs) / 1000))
    : null;

  const degradedReasons = [];
  if (!dockerAvailable) degradedReasons.push("docker unavailable");
  if (criticalServiceUnhealthy.length > 0) {
    degradedReasons.push(`critical services unhealthy (${criticalServiceUnhealthy.join(", ")})`);
  }
  if (api42Unavailable) degradedReasons.push(`api42 unavailable (${api42Status})`);

  const warningReasons = [];
  if (warningServiceUnhealthy.length > 0) {
    warningReasons.push(`warning services unhealthy (${warningServiceUnhealthy.join(", ")})`);
  }
  if (queueInternalWarn) warningReasons.push(`fetch_queue_internal high ${qInt} > ${queueThresholds.fetch_queue_internal_warn_max}`);
  if (queueExternalWarn) warningReasons.push(`fetch_queue_external high ${qExt} > ${queueThresholds.fetch_queue_external_degraded_max}`);
  if (queueProcessWarn) warningReasons.push(`process_queue high ${qProc} > ${queueThresholds.process_queue_degraded_max}`);
  if (api42TransientWarn) warningReasons.push(`api42 transient (${api42Status})`);
  if (backupStatus !== "ok") warningReasons.push(`backup status ${backupStatus}`);
  if (backupAgeSeconds !== null && backupAgeSeconds > BACKUP_WARN_AGE_SECONDS) {
    warningReasons.push(
      `backup age high ${Math.floor(backupAgeSeconds / 60)} min > ${Math.floor(
        BACKUP_WARN_AGE_SECONDS / 60
      )}`
    );
  }

  let overallStatus = "healthy";
  let overallReason = "services healthy, api42 ok, backup ok, queues ok";
  if (degradedReasons.length > 0) {
    overallStatus = "degraded";
    overallReason = degradedReasons.join(" | ");
  } else if (warningReasons.length > 0) {
    overallStatus = "warning";
    overallReason = warningReasons.join(" | ");
  }

  const drInfo = {
    runbook: "docs/DISASTER_RECOVERY_RUNBOOK.md",
    restore_command:
      "gzip -dc backups/<file>.sql.gz | docker compose -f infra/docker-compose.yml exec -T db psql -U ${DB_USER:-api42} ${DB_NAME:-api42}",
  };

  return res.json({
    timestamp_utc: new Date().toISOString(),
    global_system_health: {
      overall_status: overallStatus,
      overall_reason: overallReason,
      docker_available: dockerAvailable,
    },
    microservices_status: {
      source: microservicesSource,
      summary: servicesSummary,
      required_summary: requiredSummary,
      services: microservices,
    },
    system_resources: {
      loadavg_1: loadAvg[0],
      loadavg_5: loadAvg[1],
      loadavg_15: loadAvg[2],
      cpu_count: os.cpus().length,
      memory_total_bytes: memoryTotal,
      memory_used_bytes: memoryUsed,
      memory_free_bytes: memoryFree,
      disk_used_percent: diskUsage?.used_percent ?? null,
      disk_free_mb: diskUsage?.free_mb ?? null,
      db_size_bytes: dbSizeBytes,
    },
    api42_health: {
      status: api42Status,
    },
    queues_status: queuesStatus,
    queue_thresholds: queueThresholds,
    last_backup: {
      status: backupMeta.status || "unknown",
      timestamp_utc: backupTimestampUtc,
      age_seconds: backupAgeSeconds,
      backup_file: backupMeta.backup_file || "",
      size_bytes: backupMeta.size_bytes ?? 0,
      duration_ms: backupMeta.duration_ms ?? 0,
      retention_days: backupMeta.retention_days ?? null,
      pruned_count: backupMeta.pruned_count ?? 0,
      error: backupMeta.error || "",
    },
    dr_info: drInfo,
  });
});

const WS_OPEN = 1;
const wsClients = new Set();
let wsKnownWindowIds = new Set();
let wsScanInFlight = false;
let wsLastHeartbeatBroadcastMs = 0;

function sendWsJson(socket, payload) {
  if (!socket || socket.readyState !== WS_OPEN) return;
  try {
    socket.send(JSON.stringify(payload));
  } catch {
    // ignore transient socket send errors
  }
}

function broadcastWsJson(payload) {
  const message = JSON.stringify(payload);
  for (const socket of wsClients) {
    if (socket.readyState !== WS_OPEN) continue;
    try {
      socket.send(message);
    } catch {
      // ignore per-socket send errors
    }
  }
}

function countHealthyWsClients() {
  let count = 0;
  for (const socket of wsClients) {
    if (socket.readyState !== WS_OPEN) continue;
    if (socket.isAlive === false) continue;
    count += 1;
  }
  return count;
}

function selectReplayEnvelopes(envelopes, sinceId) {
  if (!sinceId) {
    return {
      mode: "full",
      cursor_found: null,
      events: envelopes,
    };
  }

  const idx = envelopes.findIndex((env) => env.id === sinceId);
  if (idx === -1) {
    return {
      mode: "full",
      cursor_found: false,
      events: envelopes,
    };
  }

  return {
    mode: "catchup",
    cursor_found: true,
    events: envelopes.slice(idx + 1),
  };
}

async function sendEventsSnapshot(socket, sinceId = "") {
  const snapshot = await readWindowEventEnvelopes();
  const replay = selectReplayEnvelopes(snapshot.envelopes, sinceId);
  const events = replay.events.map(toPublicWsEnvelope);
  const newestEventId = events.length > 0 ? events[events.length - 1].id : "";

  sendWsJson(socket, {
    type: "events_snapshot",
    timestamp_utc: snapshot.timestamp_utc,
    mode: replay.mode,
    cursor_found: replay.cursor_found,
    count: events.length,
    newest_event_id: newestEventId,
    events,
  });
}

async function syncAndBroadcastEventDeltas() {
  if (wsScanInFlight) return;
  wsScanInFlight = true;
  try {
    const snapshot = await readWindowEventEnvelopes();
    const currentIds = new Set(snapshot.envelopes.map((env) => env.id));
    const delta = snapshot.envelopes.filter((env) => !wsKnownWindowIds.has(env.id));
    wsKnownWindowIds = currentIds;

    if (delta.length > 0 && wsClients.size > 0) {
      broadcastWsJson({
        type: "events_delta",
        timestamp_utc: snapshot.timestamp_utc,
        count: delta.length,
        newest_event_id: delta[delta.length - 1].id,
        events: delta.map(toPublicWsEnvelope),
      });
    }

    const healthyClients = countHealthyWsClients();
    if (
      healthyClients > 0 &&
      snapshot.now_ms - wsLastHeartbeatBroadcastMs >= WS_HEARTBEAT_BROADCAST_MS
    ) {
      wsLastHeartbeatBroadcastMs = snapshot.now_ms;
      broadcastWsJson({
        type: "events_heartbeat",
        timestamp_utc: snapshot.timestamp_utc,
        connected_clients: healthyClients,
      });
    }
  } catch (err) {
    console.warn(`[WS events] scan failed: ${err.message}`);
  } finally {
    wsScanInFlight = false;
  }
}

const httpServer = app.listen(PORT, () => {
  console.log(`API listening on :${PORT}`);
});

const wsServer = new WebSocketServer({
  server: httpServer,
  path: "/ws/events",
});

wsServer.on("connection", async (socket, req) => {
  socket.isAlive = true;
  wsClients.add(socket);

  sendWsJson(socket, {
    type: "ws_ready",
    timestamp_utc: new Date().toISOString(),
    scan_interval_ms: WS_EVENTS_SCAN_MS,
  });

  const requestUrl = new URL(req.url || "/ws/events", "http://localhost");
  const sinceId = String(requestUrl.searchParams.get("since_id") || "").trim();

  try {
    await sendEventsSnapshot(socket, sinceId);
  } catch (err) {
    sendWsJson(socket, {
      type: "events_snapshot_error",
      timestamp_utc: new Date().toISOString(),
      error: err.message,
    });
  }

  socket.on("pong", () => {
    socket.isAlive = true;
  });

  socket.on("close", () => {
    wsClients.delete(socket);
  });

  socket.on("error", () => {
    wsClients.delete(socket);
  });
});

const wsScanTimer = setInterval(() => {
  void syncAndBroadcastEventDeltas();
}, WS_EVENTS_SCAN_MS);

const wsPingTimer = setInterval(() => {
  for (const socket of wsClients) {
    if (socket.isAlive === false) {
      try {
        socket.terminate();
      } catch {
        // ignore terminate failures
      }
      wsClients.delete(socket);
      continue;
    }
    socket.isAlive = false;
    try {
      socket.ping();
    } catch {
      wsClients.delete(socket);
    }
  }
}, WS_PING_INTERVAL_MS);

void (async () => {
  try {
    const snapshot = await readWindowEventEnvelopes();
    wsKnownWindowIds = new Set(snapshot.envelopes.map((env) => env.id));
  } catch (err) {
    console.warn(`[WS events] initial sync failed: ${err.message}`);
  }
})();

void syncAndBroadcastEventDeltas();

async function shutdown() {
  clearInterval(wsScanTimer);
  clearInterval(wsPingTimer);
  try {
    for (const socket of wsClients) {
      try {
        socket.close(1001, "server_shutdown");
      } catch {
        // ignore per-socket close errors
      }
    }
    wsClients.clear();
  } catch {
    // ignore ws close sweep errors
  }
  try {
    wsServer.close();
  } catch {
    // ignore ws server close errors
  }
  try {
    await dbPool.end();
  } catch {
    // ignore shutdown pool errors
  }
  httpServer.close(() => process.exit(0));
}

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
