# Backup Strategy (Current)

## Summary

Backup behavior in the current codebase is mixed:

- `ops-agent` can run a backup step.
- It only executes backup when a backup binary/script is configured and available.
- A backup script is currently not present in this repository.

Result: token refresh and log cleanup are automated; DB backup is currently skipped.

## What Data Is Recoverable from API

Safe to regenerate (API-backed metadata):
- `cursus`
- `campuses`
- `projects`
- `coalitions`
- `achievements`
- derived links generated from metadata refresh

Main restore command:

```bash
/srv/42_Network/repo/.cache/bin/toolkit-agent update_all_cursus_21_core --force
```

## What Should Be Backed Up

Should be backed up regularly once live sync matters operationally:
- PostgreSQL data volume (`data/postgres`)
- `.env` (without exposing secrets)
- `repo/.oauth_state` (secure handling required)
- Optional: key logs needed for audits/debugging

## Recommended Baseline Policy

### Daily logical backup

```bash
cd /srv/42_Network/repo
mkdir -p backups
STAMP=$(date -u +%Y%m%d_%H%M%S)
docker compose -f infra/docker-compose.yml exec -T db pg_dump -U "${DB_USER:-api42}" "${DB_NAME:-api42}" | gzip > "backups/db_${STAMP}.sql.gz"
```

### Weekly restore test

- Restore to a non-production database/container.
- Run integrity checks (table counts + spot queries).
- Verify deploy scripts still operate with restored data.

### Retention

- Daily backups: keep 14 days
- Weekly backups: keep 8 weeks
- Monthly backup: keep 6 months

## Optional Automation Script

If you want ops-agent backups to run automatically, provide a backup executable
via `BACKUP_DATABASE_BINARY` (or install `/usr/local/bin/backup-database-agent`).

## Restore Runbook (Minimal)

1. Stop writers (`pipeline_manager.sh stop` and orchestrators).
2. Restore dump into Postgres.
3. Start services.
4. Run one metadata refresh (non-force first, force if needed).
5. Validate key tables and recent user rows.

## Risk Notes

- Without periodic dumps, accidental DB volume loss means full rebuild from API + potential live-state loss.
- `.oauth_state` and `.env` are sensitive and must be protected in backup storage.
- Backups are useful even if most data is API-backed because runtime derived state and timings are not always reproducible.
