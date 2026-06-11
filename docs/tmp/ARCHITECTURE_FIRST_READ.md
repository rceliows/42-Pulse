# 42 Network Onboarding (No Server Access Yet)

Audience: new contributors who do not yet have server or runtime access.

This document gives the architecture mental model first, so you can understand the system before touching infrastructure.

## 1. What 42 Network Is

42 Network is a data ingestion and synchronization backend for 42 School data.

It has two planes:
- Stable metadata plane: slower-changing reference data (campuses, projects, coalitions, achievements)
- Live sync plane: frequent user changes (status, activity, related records)

Core idea: API data is ingested continuously, normalized, and stored so downstream apps can query consistently.

## 2. System Mental Model

```text
42 API
  -> change detection
  -> fetch detailed payloads
  -> transform + upsert into database
  -> process deferred enrichment work
  -> expose data to app/UI consumers
```

Think of it as a conveyor:
- detector decides "who changed"
- fetcher gets "full payload"
- upserter commits "canonical row state"
- backlog worker handles slower follow-up tasks

## 3. Main Runtime Roles

### Detector

Purpose:
- identify changed users since last cycle
- enqueue IDs for downstream work
- emit change events for observability

### Fetcher

Purpose:
- consume queued IDs
- call API for detailed user objects
- persist raw snapshots for traceability

### Upserter

Purpose:
- map raw payload fields to DB schema
- apply idempotent insert/update logic
- keep DB state aligned with latest known API state

### Backlog Worker

Purpose:
- process deferred/expensive related updates
- avoid blocking the main near-real-time path

### Ops + Monitoring

Purpose:
- token refresh and runtime hygiene
- health checks and queue/worker visibility
- log cleanup and operational stability

## 4. Data Lifecycle

### Stable metadata lifecycle

- periodic refresh
- validation
- upsert to canonical metadata tables

### Live user lifecycle

- detect change
- queue user ID
- fetch user detail
- upsert core user state
- enqueue/process related entities if needed

## 5. Why the Queue Pattern Exists

Queues provide:
- decoupling between detection, API fetch, and DB writes
- controlled backpressure during API spikes
- easier recovery (replay pending work)
- clear bottleneck visibility

If one stage slows down, queue size exposes where pressure accumulates.

## 6. Reliability Principles Used

- Idempotent writes: reruns should converge, not duplicate
- Separation of concerns: each worker has a single main responsibility
- Snapshot preservation: raw fetched data is kept for debugging/replay
- Operational observability: logs + queue depth + worker status

## 7. Common Failure Modes (Conceptual)

### Fetch queue grows continuously

Likely causes:
- API throttling
- fetch worker lag
- bursty detector output

### Process queue grows continuously

Likely causes:
- DB-side write slowdowns
- upserter failures
- schema mismatch after payload changes

### Frequent auth errors

Likely causes:
- token lifecycle issues
- expired refresh credentials

## 8. Vocabulary You Should Know

- Upsert: insert-or-update behavior
- Backpressure: downstream stages cannot keep up with input rate
- Delta sync: processing only changed records
- Canonical schema: normalized DB model used as source of truth internally
- Enrichment: follow-up updates for related entities after core user sync

## 9. What To Read Next (Still No Server Needed)

1. `PIPELINE_FLOW.md` (expanded flow walkthrough)
2. `repo/docs/21_PIPELINE_VISUAL_GUIDE.md` (visual diagrams)
3. `repo/docs/20_CURSUS_21_DATA_PIPELINE.md` (deeper design details)

## 10. Once Access Is Granted

When you later get environment access, switch to:
- `DEPLOYMENT_GUIDE.md` for setup
- `repo/docs/12_COMMAND_REFERENCE.md` for operational commands

This keeps onboarding split cleanly:
- now: architecture understanding
- later: execution and operations
