# Test Case Catalogue

| ID | Scenario | Input / action | Expected result |
|---|---|---|---|
| CFG-01 | Valid config | Five valid fields | Load succeeds |
| CFG-02 | Quoted argument | Argument contains a space | Single argv token |
| CFG-03 | Missing dependency | Unknown dependency name | Load fails |
| CFG-04 | Dependency cycle | A -> B -> A | Load fails |
| CFG-05 | Duplicate service | Same name twice | Load fails |
| CFG-06 | Unterminated quote | Missing closing quote | Load fails |
| CFG-07 | Missing configuration | Nonexistent path | Load fails safely |
| CFG-08 | Invalid restart count | Non-numeric count | Load fails |
| CFG-09 | Invalid restart delay | Non-numeric delay | Load fails |
| CFG-10 | Empty command | Empty command field | Load fails |
| CFG-11 | Empty process name | Empty name field | Load fails |
| CFG-12 | Malformed field count | Missing pipe-separated field | Load fails |
| CFG-13 | Argument limit | More than maximum supported arguments | Load fails |
| CFG-14 | Escaped whitespace | Backslash-escaped space | One argument token |
| CFG-15 | Comments and blank lines | Comment/empty records | Valid records still load |
| CFG-16 | Dependency readiness | Dependency stopped then running | Readiness changes from false to true |
| API-01 | Null API arguments | Null supervisor/process arguments | Safe error/return without crash |
| INT-01 | Reverse dependency listing | App before worker before base | Base starts first |
| INT-02 | Abnormal service exit | `/bin/false`, retry limit 2 | Exactly three launches |
| INT-03 | Clean service exit | `/bin/true` | No restart scheduled |
| OPS-01 | Resource sampling | Keep `/bin/sleep` alive | VmRSS log records appear |
| OPS-02 | Graceful shutdown | Send SIGTERM | Supervisor exits and reaps children |
| OPS-03 | Daemon launch | Use `--daemon` | Detached process logs to file |
| OPS-04 | Watchdog heartbeat | Normal supervisor loop | Watchdog receives periodic pipe data |

Run automated tests with `make test-gtest`, `make test-integration`, `make test-restart`, and `make test-startup-errors` (or `make test` for all). Operational cases require observing the actual runtime log and process state.
