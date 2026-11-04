# Evaluation Rubric Mapping

| Evaluation area | Project evidence / location | Validation or presentation evidence |
|---|---|---|
| Scrum / product backlog | `docs/PROJECT_BACKLOG.md` | Sprint plan, completed items and follow-on backlog |
| POC demonstration | `README.md`, `config/services.conf` | Launch, dashboard, resource logs, restart and shutdown demonstration |
| Memory management | `src/config.c`, `src/supervisor.c` | Dynamic argv/dependency allocation and cleanup; `make valgrind` report |
| Code optimization | `src/supervisor.c`, `src/procmon.c` | Non-blocking `waitpid`, bounded polling, resource monitor interval |
| Secure coding / MISRA C | strict compiler flags, parser validation, `make misra` | Review Cppcheck/MISRA report and document justified deviations |
| Test-case review | `docs/TEST_CASES.md`, `tests/` | GoogleTest and shell integration outputs |
| HLD / LLD | `docs/HLD_LLD.md` | Architecture, module responsibilities, state model and synchronization |
| UML diagrams | Mermaid diagrams in `docs/HLD_LLD.md` | Render flow and state diagrams in a Markdown viewer |
| Code analysis / coverage | Makefile `cppcheck`, `coverage` targets | Retain Cppcheck and gcov outputs |
| Multithreading | `src/procmon.c`, watchdog in `src/main.c` | Resource-monitor and watchdog threads; `make helgrind` report |
| Timestamping / logging | `src/logger.c` | Timestamped `logs/supervisor.log` |
| Coding / compiling / debugging | `Makefile`, source tree | `make clean all`; use `make debug` and GDB breakpoints |

## GDB quick-start

```bash
make debug
gdb --args ./process_supervisor config/services.conf
```

Useful GDB commands: `break main`, `break start_process`, `run`, `next`, `step`, `print supervisor`, `info threads`, `thread apply all bt`, `continue`, `quit`.
