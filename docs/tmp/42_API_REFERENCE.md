# 42 API Reference (Used by 42_Network)

Scope: practical endpoint reference for active C++ agents under `repo/app/*`.

## Base URL and Auth

- API root: `https://api.intra.42.fr`
- Auth: OAuth2 bearer token
- Header: `Authorization: Bearer <token>`

Token helper used by this project:

```bash
/srv/42_Network/repo/.cache/bin/token-manager-agent exchange "<AUTH_CODE>"
/srv/42_Network/repo/.cache/bin/token-manager-agent refresh
/srv/42_Network/repo/.cache/bin/token-manager-agent call /v2/me
/srv/42_Network/repo/.cache/bin/token-manager-agent call-export "/v2/cursus/21" /tmp/cursus21.json
```

## Rate Limits

Project assumptions:
- Burst: `2 requests/second`
- Sustained: `1200 requests/hour`

Headers to watch:
- `X-RateLimit-*`
- `Retry-After` (when throttled)

## Pagination and Filtering

Common query patterns:

```text
per_page=100&page=1
filter[field]=value
range[field]=start,end
sort=field,-other_field
```

Most fetch agents use paginated loops and stop when an empty page is returned.

## Endpoints Used in Active Agents

### Metadata

- `GET /v2/cursus/21`
- `GET /v2/campus?filter[active]=true&filter[public]=true&per_page=100&page=N`
- `GET /v2/cursus/21/projects?per_page=100&page=N`
- `GET /v2/cursus/21/achievements?per_page=100&page=N`
- `GET /v2/coalitions?per_page=100&page=N`

Used by:
- `repo/.cache/bin/toolkit-agent fetch_metadata`
- `repo/toolkit-agent fetch_cursus_21_core_metadata`

### Users and Live Sync

- `GET /v2/campus/{id}/users?page=N&per_page=100`
- `GET /v2/cursus/21/cursus_users?filter[kind]=student&filter[alumni?]=false&page=N&per_page=100`
- `GET /v2/users/{id}`
- `GET /v2/users/{id}/projects_users`

Used by:
- `repo/.cache/bin/toolkit-agent fetch_users`
- `repo/.cache/bin/detector-agent --loop`
- `repo/.cache/bin/fetcher-agent`
- `repo/.cache/bin/backlog-worker-agent`

## Response Fields Frequently Used

From user/cursus-user payloads:
- `id`, `login`, `email`, `first_name`, `last_name`
- `kind`, `alumni` or `alumni?`
- `campus_users[]`
- `cursus_users[]`
- `projects_users[]`
- `achievements[]`
- `updated_at`

## Common API Failure Modes

- `401`: token expired/invalid -> refresh and retry
- `403`: missing scope or app restriction
- `429`: too many requests -> backoff and retry
- `5xx`: transient API issue -> retry with capped attempts

## Operational Notes

- Use conservative sleep intervals in loops for long jobs.
- Prefer incremental windows (`updated_at`) for recurring sync.
- Keep per-campus jobs isolated when debugging data mismatches.
- Always inspect saved JSON in `exports/` before DB-load troubleshooting.

## Quick Validation Commands

```bash
# API identity
/srv/42_Network/repo/.cache/bin/token-manager-agent call /v2/me | jq '{id,login}'

# Cursus metadata
/srv/42_Network/repo/.cache/bin/token-manager-agent call "/v2/cursus/21" | jq '{id,name}'

# Sample campus users page
/srv/42_Network/repo/.cache/bin/token-manager-agent call "/v2/campus/76/users?per_page=5" | jq 'length'
```
