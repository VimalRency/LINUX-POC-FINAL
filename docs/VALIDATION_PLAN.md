# Validation Plan and Evidence Checklist

| Requirement | Validation | Expected result |
|---|---|---|
| Strict compilation | `make clean all` | GCC completes with `-Werror` |
| Configuration parsing | `make test-gtest` | All GoogleTest cases pass |
| Quoted arguments | `LoadsQuotedArgumentsAndDependencies` | Space-containing argument remains one argv entry |
| Unknown dependency | `RejectsUnknownDependency` | Config rejected |
| Dependency cycle | `RejectsDependencyCycle` | Config rejected |
| Duplicate service | `RejectsDuplicateNames` | Config rejected |
| Unterminated quote | `RejectsUnterminatedQuote` | Config rejected |
| Dependency order | `make test-integration` | Base, worker, app start in dependency order |
| Restart policy | `make test-restart` | Initial launch plus configured retry count; clean exit not restarted |
| Resource monitor | Run supervisor and inspect log | VmRSS values logged for live child processes |
| IPC watchdog | Observe heartbeat during normal run | Watchdog remains alive while supervisor loop emits heartbeats |
| Shutdown | Send SIGINT/SIGTERM | Children terminated/reaped; shutdown logged |
| Daemon mode | `./process_supervisor --daemon /absolute/path/services.conf` | Process detaches and logs to file |
| Memory analysis | `make valgrind` | Review leak/error summary; require zero errors for pass |
| Thread analysis | `make helgrind` | Review race report; investigate all reported races |
| Static analysis | `make cppcheck` | Resolve all enabled findings; retain output |
| MISRA analysis | `make misra MISRA_ADDON=...` | Review findings and deviations; no certification implied |
| Coverage | `make coverage` | gcov report generated; assess uncovered paths |

## Test evidence record

Record the date, Linux distribution, GCC/Cppcheck/Valgrind/GoogleTest versions, command, exit status and complete output for every run. A target is considered passed only when it returns zero and its report has been reviewed. Do not infer a pass from compilation or from another machine.
