# Step 1 verification results

Verification date: **1 November 2026**, Asia/Calcutta (UTC+05:30). These checks were performed by the AI agent in WSL, not personally by the student. Student verification remains pending.

## Environment

- Windows workspace: `D:\Emai_service_project`.
- Linux workspace: `/mnt/d/Emai_service_project`.
- Distribution: explicitly selected `Ubuntu-24.04`, Ubuntu 24.04.1 LTS.
- Kernel: WSL2, `6.6.87.2-microsoft-standard-WSL2`, x86_64.
- GCC 13.3.0, GNU Make 4.3, Git 2.43.0, Python 3.12.3.
- GDB: not installed; no interactive GDB debugging was performed.

The first environment probe used the system's default Docker distribution and could not find the project mount there. Selecting Ubuntu-24.04 resolved this. The final application builds and tests all ran in Ubuntu; no packages, Windows features, or global Git settings were changed.

## Build and execution results

Open Ubuntu from PowerShell with `wsl -d Ubuntu-24.04`. From `/mnt/d/Emai_service_project` inside Ubuntu, these commands were actually executed:

| Command | Observed result | Retained output |
|---|---|---|
| `make clean` | Exit 0; removed generated build outputs | [Cleanup](evidence/clean.txt) |
| `make all debug sanitize` | Exit 0; all three modes compiled without warning diagnostics | [Build](evidence/build.txt) |
| `make test` | 18 tests passed in 11.505 seconds | [Normal tests](evidence/normal-tests.txt) |
| `make MODE=debug test` | 18 tests passed in 11.354 seconds | [Debug tests](evidence/debug-tests.txt) |
| `make MODE=sanitize test` | 18 tests passed in 12.958 seconds | [ASan and UBSan tests](evidence/sanitize-tests.txt) |

This is **54 passing test-method executions across three build modes**, with additional cases inside those methods. It is not 54 distinct scenarios. Timings above are the unittest runner's reported durations; command-wrapper timings in [the evidence index](evidence/summary.json) also include build/launch overhead.

The sanitizer build uses AddressSanitizer and UndefinedBehaviorSanitizer with recovery disabled and frame pointers retained. Its tests executed the instrumented binaries, not normal binaries. No sanitizer diagnostics were reported. Initial normal and sanitizer runs also passed before the final recorded verification; the retained outputs above are the later recorded runs.

Additional GCC checks use `-std=c11 -Wall -Wextra -Wpedantic -fanalyzer` while compiling each source file to an object. Exact commands and observed diagnostics are retained in [server analysis](evidence/analysis-server.txt), [client analysis](evidence/analysis-client.txt), and [shared I/O analysis](evidence/analysis-net_io.txt). An earlier syntax-only command is retained separately in [syntax-check output](evidence/syntax-check.txt); syntax-only checking is not treated as a full static-analysis pass.

## Behaviour covered

1. Several C client processes exchange matching messages with one server without restarting it.
2. A binary payload containing nulls, a line break, separator, format-string characters, and a non-ASCII byte is echoed exactly.
3. A header and body sent byte by byte are reconstructed correctly.
4. A complete frame sent in one buffer is handled correctly.
5. One-byte and 4096-byte payloads succeed, including a maximum-size C CLI message.
6. Unicode CLI messages use byte lengths and round-trip correctly.
7. Zero, oversized, and maximum-uint32 declared lengths are rejected; valid exchanges prove recovery afterwards.
8. EOF before a frame, in a partial header, before a body, and in a partial body are distinguished and handled without terminating the server.
9. A reset connection before reading the response does not stop later valid exchanges; no particular timing-dependent socket error is required.
10. Stalled requests time out and the server then serves a healthy client.
11. A second server fails clearly when the address/port is occupied; the original still serves requests.
12. Invalid arguments cover empty/out-of-range/negative/non-numeric ports, overflow, invalid IPv4, IPv6, unknown/missing/duplicate options, extra positional input, and invalid message lengths.
13. Both programs provide working help output.
14. A reserved but non-listening port causes a clear client connection failure.
15. Controlled TCP peers send mismatched, invalid-length, incomplete, and absent responses; the C client rejects them.
16. A peer withholding its response triggers the C client's receive timeout.
17. SIGINT and SIGTERM stop an idle server within the two-second test bound.
18. SIGINT and SIGTERM stop a server with a stalled connection within the seven-second test bound.

## Evidence integrity and limits

The `evidence/` text files contain actual commands, exit statuses, captured stdout/stderr, timestamps, and wrapper durations. [summary.json](evidence/summary.json) indexes these records. Environment-version outputs are retained alongside them. Test subprocess logs are inspected during each test and temporary per-server logs are removed by the harness; the retained suite logs contain the actual test outcomes, not every individual server transcript.

No screenshots or results were fabricated. Automated execution does not establish the student's ability to explain the code. The student still needs to run the README demonstration and practise the architecture questions before recording personal verification.

The test suite has not certified simultaneous request handling, LAN deployment, accounts, storage, encryption, full resource-exhaustion recovery, every partial-send condition, or resistance to indefinitely progressing slow clients. The server is sequential, receive/send limits are inactivity waits, and `connect()` uses the OS default timeout. Passing checks reduce known risk; they do not prove the program is failproof.
