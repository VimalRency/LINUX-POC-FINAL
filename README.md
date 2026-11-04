# ADAS Linux Process Supervisor — Mini-init POC

A Linux/POSIX C11 proof of concept that launches and supervises configured ADAS-related services. It is an educational POC and is **not a production automotive safety component**.

## Implemented capabilities

- Configuration-driven service launch with `fork()` and `execvp()`.
- Child exit detection using `waitpid(..., WNOHANG)`.
- Bounded automatic restart for non-zero exits and signal terminations; clean exit (`0`) is not restarted.
- Dependency validation: unknown dependency, duplicate service and dependency-cycle rejection.
- Dependency-aware startup, including configurations listed in reverse dependency order.
- Linked-list process table protected by a POSIX mutex.
- Timestamped, mutex-protected log output.
- Resource-monitor thread snapshots the process table under a mutex, then reads `/proc/<pid>/status` outside the critical section and logs resident memory (VmRSS).
- IPC watchdog: the supervisor loop sends heartbeat bytes through a pipe; a watchdog thread polls for heartbeat timeout and requests shutdown if heartbeats stop.
- Optional daemon mode (`--daemon`).
- SIGINT/SIGTERM/SIGHUP handling and graceful child shutdown with a bounded SIGKILL fallback.
- Status dashboard printed periodically in foreground mode.
- Quoted command arguments and backslash escaping in configuration.

## Configuration format

Each non-comment line contains five pipe-separated fields:

```text
name|max_restarts|restart_delay_seconds|dependencies|command and arguments
```

Use `-` when there are no dependencies. Separate multiple dependencies with commas. Quote arguments containing spaces.

```text
database|3|2|-|/bin/sleep 300
sensor|3|2|database|/bin/sleep 300
camera|3|2|database|/bin/sleep 300
adas_manager|3|2|sensor,camera|/bin/sleep 300
```

`max_restarts=3` means one initial launch plus at most three retries. Commands are executed directly; pipes, redirects and shell operators are not interpreted. Use `/bin/sh -c 'command string'` only when shell behavior is deliberately needed.

## Build and run (Ubuntu / WSL)

```bash
sudo apt update
sudo apt install build-essential make gdb cppcheck valgrind libgtest-dev gcovr
cd /mnt/c/Users/matha/OneDrive/Desktop/POC/LINUX_Final
make clean
make
./process_supervisor config/services.conf
```

Stop foreground mode with Ctrl+C. Daemon mode:

```bash
./process_supervisor --daemon config/services.conf
```

Daemon mode redirects standard streams and continues logging to `logs/supervisor.log`. Prefer an absolute configuration path when launching from another working directory.

## Validation commands

```bash
make test-gtest          # GoogleTest configuration unit tests
make test-integration    # dependency startup order integration test
make test-restart        # bounded restart and clean-exit integration test
make test-startup-errors # invalid CLI/configuration failure paths
make test                # all unit/integration tests (requires GoogleTest)
make cppcheck            # warning/style/performance/portability analysis
make valgrind            # Memcheck of supervisor process (child execs are not traced)
make helgrind            # Helgrind of supervisor threads (child execs are not traced)
make coverage            # gcov instrumentation, unit and integration tests, line/branch report
make misra MISRA_ADDON=/path/to/misra.py
```

Install packages on Ubuntu/WSL with `sudo apt install build-essential make gdb cppcheck valgrind libgtest-dev gcovr`. The MISRA addon path varies by distribution. The MISRA target requires a compatible Cppcheck MISRA addon path.

## Important validation notes

A successful build does not prove absence of leaks, races, or MISRA deviations. Run every analysis target on the evaluation machine and retain its real output. This package has been built with GCC strict warnings as errors, and the dependency and restart integration scripts have been executed successfully in the package build environment. GoogleTest, Cppcheck, Valgrind, Helgrind and the MISRA addon are environment-dependent; do not claim their pass status until the corresponding target succeeds on your WSL/Linux installation.

## Known POC boundaries

- Dependency-aware initial startup is implemented. Full dependency failure propagation (automatically stopping/restarting already-running dependents when an upstream dependency fails) is not yet implemented.
- Resource telemetry currently records resident memory, not CPU utilization, I/O or thermal metrics.
- The IPC watchdog monitors the supervisor main-loop heartbeat; it is not a per-service application heartbeat protocol.
- No privilege dropping, cgroup/resource-limit enforcement, log rotation, service health endpoint, or safety certification is provided.
- POSIX/Linux APIs are used; this is not a strictly freestanding MISRA-C implementation. Treat MISRA addon output as findings to review, not a certification claim.
"# LINUXPOC" 
