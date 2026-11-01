# Email service networking foundation

This is Step 1 of the C email-service assignment. A terminal client sends a framed message to a TCP server, verifies the exact echoed response, and disconnects. The server continues accepting connections after normal disconnects and invalid requests.

The server processes one client at a time. Accounts, email operations, persistent storage, and simultaneous client handling are later milestones. This software is a local educational prototype, not a production email server.

## Environment and prerequisites

Verified on 1 November 2026 using WSL2 and **Ubuntu-24.04**, Ubuntu 24.04.1 LTS, GCC 13.3.0, GNU Make 4.3, Git 2.43.0, and Python 3.12.3. Application code is C11 with Linux/POSIX socket APIs; tests use only Python's standard library. No third-party application library is required.

The Windows project directory is `D:\Emai_service_project`. Its verified Ubuntu path is `/mnt/d/Emai_service_project`.

**PowerShell:** open the correct distribution explicitly. This system's default WSL distribution is Docker's environment.

```powershell
wsl -d Ubuntu-24.04
```

**Inside Ubuntu:** enter the project and build.

```sh
cd /mnt/d/Emai_service_project
make
```

GCC, Make, Git, and Python are already available in the verified environment. GDB was not installed and was not needed for this milestone's verification. If you want an interactive debugger, install it inside Ubuntu:

```sh
sudo apt update
sudo apt install gdb
```

For a fresh Ubuntu installation missing the build/test prerequisites:

```sh
sudo apt update
sudo apt install build-essential git python3
```

These installation commands are setup instructions, not a claim that packages were installed during development. No Windows feature changes or reboot are required for the already-working environment.

## Run the demonstration

In the first Ubuntu terminal:

```sh
cd /mnt/d/Emai_service_project
./build/normal/server --bind 127.0.0.1 --port 5050
```

The server reports `Listening on 127.0.0.1:5050` after successful startup. In a second Ubuntu terminal:

```sh
cd /mnt/d/Emai_service_project
./build/normal/client --host 127.0.0.1 --port 5050 --message "Hello from the client"
./build/normal/client --host 127.0.0.1 --port 5050 --message "A second connection"
```

Each successful client prints its echoed byte count and `response matches`. The server reports connection activity and echoed byte counts, not message contents. Both executables provide `--help`. Options shown above are optional except the client's `--message`; the default address and port are `127.0.0.1` and `5050`.

Press Ctrl+C in the server terminal to stop it. SIGTERM is also handled. Idle shutdown normally takes less than 250 ms. If a stop signal arrives immediately before an I/O call, shutdown may wait for that call's five-second timeout; the local automated check allows seven seconds for scheduling overhead.

## Build and test

Run the following inside Ubuntu from the project root:

```sh
make                       # Optimised build in build/normal
make debug                 # Debug symbols in build/debug
make sanitize              # ASan/UBSan build in build/sanitize
make test                  # Integration suite against normal executables
make MODE=debug test       # Same suite against debug executables
make MODE=sanitize test    # Same suite against instrumented executables
make test TEST_ARGS="--port 55050"  # Optional available fixed test port
make clean                 # Delete only the project's build directory
```

Normal, debug, and sanitizer outputs are separate. Changes to the Makefile trigger recompilation. If you manually change compiler variables or flags within the same mode, run `make clean` before rebuilding. Do not run clean concurrently with builds or tests. The Makefile intentionally uses tabs for recipes; C and Python use four-space indentation.

The integration suite launches real server and client processes, uses loopback TCP, and checks actual responses. It selects available ports by default and cleans up its own processes. An explicitly selected test port must be free. A small bind race remains possible when choosing an ephemeral port; startup failures are reported, not concealed or resolved by killing other processes.

See [test results](docs/test-results.md) for the actual verification commands and recorded outcomes. Sanitizers detect many memory/undefined-behaviour defects; passing them is not proof that all defects are absent.

## How the project fits together

| Path | Purpose |
|---|---|
| `src/server.c` | Server options, listener, shutdown, and one-client handling |
| `src/client.c` | Client options, connection, request, and exact response verification |
| `src/net_io.c` | Shared address parsing, timeouts, byte transfers, and framing |
| `include/net_io.h` | Shared limits, statuses, and helper contracts |
| `tests/test_network.py` | Standard-library integration test harness |
| `Plan/Step1.md` | Retained implementation prompt |
| `docs/` | Protocol, architecture, evidence, and responsible AI-use records |

Start with [architecture and viva notes](docs/architecture.md), then read the [wire protocol](docs/protocol.md). AI assistance is disclosed in the [AI usage log](docs/ai-usage-log.md); automated checks and personal student verification are recorded separately.

Git was initialised locally during Step 1. The student subsequently committed the project and connected `origin` to `https://github.com/Aparup2139/Email_Service.git`; the working branch is now `main`. Build outputs are ignored; source, documentation, tests, and logs remain visible to Git.

The next milestone is specified in [the Step 2 implementation plan](Plan/Step2.md). It is a plan, not a claim that account features already exist.

## Errors and current limits

- `connect: Connection refused`: start the server and check that both programs use the same address/port and WSL distribution.
- `bind: Address already in use`: stop your own earlier server or choose another port. Do not kill unrelated processes.
- Invalid argument errors: use numeric IPv4, a decimal port from 1 to 65535, and a message from 1 to 4096 bytes. Quote messages containing spaces. Hostnames and IPv6 are unsupported.
- Frame errors close only the affected connection. There is no structured protocol error response yet.
- Receive and send operations have five-second inactivity waits after connection setup. A slow peer that keeps making progress can extend the overall exchange; there is no total-frame deadline.
- `connect()` uses the OS default timeout. A remote unreachable address can take much longer than five seconds to fail.
- One client can occupy the sequential server until it finishes or times out. A listen backlog queues connections; it does not provide concurrent request handling.
- Messages are at most 4096 bytes, one request and response per connection. The command line cannot carry embedded null bytes, but framing supports arbitrary bytes, as tested with raw TCP peers.
- Traffic is unencrypted and unauthenticated. Bind to loopback for this milestone. There is no user database or application data storage; `build/` contains generated programs, not mail data.
- The test suite does not certify LAN deployment, all resource-exhaustion conditions, or the complete future assignment. No fabricated screenshots or student verification are included.

## Practice before the viva

Run the two-client demonstration, stop the server, and try the client again. Explain why the first two clients succeed while the final one fails. Then trace `handle_client()`, `recv_frame()`, and `recv_exact()` and explain why a four-byte header may require several receives.
