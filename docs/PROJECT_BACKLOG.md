# POC Backlog and Sprint Plan

## Product goal
Demonstrate a Linux C process supervisor suitable as a learning POC for managing a small set of ADAS-related services.

## Completed in this POC
- [x] Config-driven launch and child monitoring
- [x] Bounded restart policy
- [x] Dependency graph validation and ordered startup
- [x] Timestamped logging and status display
- [x] `/proc` resident-memory sampling
- [x] Mutex-protected process table and logger
- [x] Signal-based shutdown
- [x] IPC pipe heartbeat and watchdog thread
- [x] Optional daemon mode
- [x] Unit and shell integration test scaffolding

## Follow-on backlog
- [ ] Dependency failure propagation to already-running dependents
- [ ] CPU utilization sampling and threshold alerts
- [ ] Process groups and descendant cleanup
- [ ] Log rotation and configurable log levels
- [ ] Privilege separation and Linux resource controls (cgroups/rlimits)
- [ ] Per-service heartbeat protocol and health checks
- [ ] Expanded fault-injection and concurrency tests
- [ ] MISRA deviation review and project-specific compliance profile

## Suggested sprint sequence
1. **Sprint 1 — Core lifecycle:** configuration, launch, wait/reap, restart and shutdown.
2. **Sprint 2 — Observability:** status dashboard, timestamps, resource sampling and watchdog.
3. **Sprint 3 — Reliability:** dependency validation, integration tests, memory/thread analysis.
4. **Sprint 4 — Hardening:** privilege controls, process groups, fault injection, MISRA review and coverage improvement.

Review each backlog item against a demonstrated test and retain tool output as evidence. This plan is a POC work plan, not evidence that every follow-on item is implemented.
