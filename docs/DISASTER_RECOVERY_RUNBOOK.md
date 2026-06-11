# Disaster Recovery Runbook

## Scope

This runbook restores PostgreSQL from a backup produced by `ops-agent backup`.

## Preconditions

- Stack deployed from `/srv/42_Network/repo`.
- Backup file available under `/srv/42_Network/backups/` (default).
- Access to Docker and `docker compose`.

## 1. Stop writers

```bash
cd /srv/42_Network/repo
make stop-detector
docker compose -f infra/docker-compose.yml stop fetcher_internal fetcher-external-1 fetcher-external-2 fetcher-external-3 upserter-users upserter-events
```

## 2. Pick backup file

```bash
ls -lh /srv/42_Network/backups/db_*.sql.gz | tail -n 20
```

Example variable:

```bash
BACKUP_FILE="/srv/42_Network/backups/db_YYYYMMDD_HHMMSS.sql.gz"
```

## 3. Restore into DB

```bash
cd /srv/42_Network/repo
gzip -dc "$BACKUP_FILE" | docker compose -f infra/docker-compose.yml exec -T db \
  psql -U "${DB_USER:-api42}" "${DB_NAME:-api42}"
```

## 4. Validate restore

```bash
docker compose -f infra/docker-compose.yml exec -T db \
  psql -U "${DB_USER:-api42}" "${DB_NAME:-api42}" -c "SELECT NOW();"

docker compose -f infra/docker-compose.yml exec -T db \
  psql -U "${DB_USER:-api42}" "${DB_NAME:-api42}" -c "SELECT COUNT(*) FROM users;"
```

## 5. Restart services

```bash
cd /srv/42_Network/repo
docker compose -f infra/docker-compose.yml up -d fetcher_internal fetcher-external-1 fetcher-external-2 fetcher-external-3 upserter-users upserter-events
make up-detector
```

## 6. Post-restore checks

```bash
cd /srv/42_Network/repo
../runtime/cache/bin/ops-agent system_health
```

Check:
- all core containers running
- queues moving (not blocked)
- API and UI endpoints reachable
