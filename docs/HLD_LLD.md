# High-Level and Low-Level Design

## High-level design

The supervisor loads a service configuration, validates names and dependency graph, launches services in dependency order, monitors child state, applies bounded restart policy, and performs graceful shutdown. A separate resource-monitor thread reads `/proc`; a watchdog thread monitors the main-loop heartbeat received over a pipe.

```mermaid
flowchart TD
    CFG[services.conf] --> PARSER[Config Parser and Validator]
    PARSER --> TABLE[Linked-list Process Table]
    TABLE --> LAUNCH[Process Launcher: fork / execvp]
    LAUNCH --> CHILD[Managed Service Processes]
    CHILD --> WAIT[waitpid Monitor]
    WAIT --> POLICY[Restart and Dependency Policy]
    POLICY --> TABLE
    TABLE --> DASH[Status Dashboard]
    TABLE --> PROC[Resource Monitor: /proc]
    LOOP[Supervisor Main Loop] --> PIPE[Heartbeat Pipe]
    PIPE --> WD[Watchdog Thread]
    TABLE --> LOG[Timestamped Logger]
    PROC --> LOG
    WD --> LOG
```

## Low-level modules

| Module | Responsibility |
|---|---|
| `main.c` | CLI, signal handlers, optional daemonization, thread lifecycle and heartbeat pipe setup |
| `config.c` | Parse pipe-delimited records, tokenize quoted command arguments, parse dependencies, reject invalid graphs |
| `supervisor.c` | Process table, launch, dependency readiness, `waitpid`, restart scheduling, status and shutdown |
| `procmon.c` | Periodic `/proc/<pid>/status` VmRSS collection |
| `logger.c` | Timestamped serialized file logging |
| `tests/test_config.cpp` | GoogleTest configuration unit tests |
| `tests/integration_dependencies.sh` | Dependency ordering integration test |
| `tests/integration_restart.sh` | Restart-limit and clean-exit integration test |

## Core data structures

`process_t` stores service name, command, argv, dependency names, restart configuration, PID, state, exit status, next restart time and linked-list pointer. `supervisor_t` owns the list, mutex, shutdown flag, heartbeat write descriptor and log path.

## State model

```mermaid
stateDiagram-v2
    [*] --> STOPPED
    STOPPED --> RUNNING: dependencies ready / launch
    STOPPED --> WAITING_DEPENDENCY: dependencies not ready
    WAITING_DEPENDENCY --> RUNNING: dependencies become ready
    RUNNING --> STOPPED: exit status 0
    RUNNING --> WAITING_RESTART: abnormal exit / retries remain
    WAITING_RESTART --> RUNNING: delay elapsed and dependencies ready
    RUNNING --> PERMANENTLY_FAILED: abnormal exit / retry limit exhausted
    WAITING_RESTART --> PERMANENTLY_FAILED: retry launch limit exhausted
```

## Synchronization and process safety

The process list and mutable process state are accessed under the supervisor mutex. The logger uses its own mutex. Heartbeat delivery uses a POSIX pipe; the watchdog polls the read end with a timeout. Child processes are launched with `execvp`, and the parent reaps them with `waitpid`. Shutdown sends SIGTERM, waits for a bounded interval, then sends SIGKILL to remaining direct child PIDs.
