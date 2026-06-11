# Orchestra Completion Notes

- **Scope**: One-shot deploy bootstrap (metadata fetch/load, optional internal users fetch/load, optional worker start).
- **Config**: Knobs in `app/orchestra/config/orchestra.conf` are now limited to used keys only.
- **Run**: `./.cache/bin/toolkit-agent orchestra` (uses config defaults).
- **Worker**: Managed via `toolkit-agent backlog_worker_manager {start|stop|status}`.
