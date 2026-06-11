# Automated Maintenance (Backups + Cleanup + Health)

This project uses a dedicated containerized scheduler service:

- service: `maintenance-scheduler`
- binary entrypoint: `/usr/local/bin/maintenance-scheduler-agent`
- managed through `make maintenance-auto-*`

Each cycle runs:
- token refresh
- database backup
- logs cleanup
- fresh system health snapshot

No command in this flow removes `../data/postgres` or uses `fclean`.

## Commands

```bash
# start/reconcile scheduler container (default every 3600s = 1h)
make maintenance-auto-start

# stop scheduler container
make maintenance-auto-stop

# scheduler container status + latest state file
make maintenance-auto-status

# run one cycle immediately (maintenance + cleanup)
make maintenance-auto-once
```

## Optional tuning

```bash
# every hour
make maintenance-auto-start AUTO_MAINTENANCE_INTERVAL_S=3600

# optional compose orphan cleanup each cycle (disabled by default)
make maintenance-auto-start AUTO_MAINTENANCE_REMOVE_ORPHANS=1
```

## Runtime files

- PID (container process): `../runtime/logs/pids/maintenance_scheduler_container.pid`
- Log: `../runtime/logs/ops/scheduler.log`
- State: `../runtime/logs/state/scheduler_state.json`
