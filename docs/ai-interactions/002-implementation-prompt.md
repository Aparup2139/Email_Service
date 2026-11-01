# Step 1 Implementation Prompt

## Your task

Work in `D:\Emai_service_project` and implement the first milestone of an individual C socket programming assignment: a small TCP server and client that exchange a message. Set up the project structure, compiler workflow, Git, Makefile, documentation, meaningful tests, and an honest AI usage log.

The student must be able to understand and explain every part of the result. Build a reliable, understandable foundation with explicit limits and verified behaviour. No program is failproof: report actual limitations and unverified assumptions instead of making absolute reliability claims.

Complete the implementation and available verification. Do not stop after proposing a plan. Preserve this prompt and all existing user files.

## Assignment context and scope

The eventual project is a miniature email service written primarily in C, using TCP/IP sockets. It will support registration, authentication, sending to one or more recipients, inbox and sent mail, persistent storage, and multiple clients. At least three simultaneous clients must eventually be demonstrated.

This milestone implements only the networking foundation. Its acceptance condition is:

> A client connects, sends a framed message, receives and verifies the echoed response, and disconnects. The server remains running and serves subsequent connections, including after a malformed or interrupted connection.

Do not implement accounts, passwords, email storage, inboxes, a GUI, SMTP, IMAP, TLS, or a database yet. Do not add empty placeholder modules for those features. Explain that this first server handles one client at a time; concurrent clients belong to a later milestone. Keep connection handling in a separate function so concurrency can be added later without rewriting the protocol.

## 1. Inspect the environment before changing it

1. Confirm the intended project directory and inspect its existing files.
2. Read applicable `AGENTS.md` instructions, including instructions in parent directories and any directory being modified.
3. Check whether this directory already belongs to a Git repository. Preserve existing history and uncommitted work. Do not initialise a nested repository inside an existing repository.
4. Determine whether WSL and an Ubuntu/Linux distribution are already installed and usable. Check available GCC, Make, Git, GDB, and Python 3 inside that Linux environment. Python is for test orchestration only; the application must remain C.
5. Prefer Linux/POSIX sockets using WSL on this Windows machine. Use the Linux representation of the project path, typically `/mnt/d/Emai_service_project`, after verifying it. Run compilation and networking tests in the same Linux environment.
6. Do not assume PowerShell executes Linux commands directly. Clearly distinguish PowerShell commands from commands run inside WSL.
7. If required tools are missing, install them only within the available authorisation and execution permissions. Do not change system settings, enable Windows features, reboot, or launch elevated installers without authorisation. If setup requires user action, finish independent source/documentation work and provide exact setup commands with the missing prerequisite identified.
8. Do not replace the planned POSIX implementation with Winsock silently. If Linux execution is unavailable, provide the intended POSIX source and state which checks remain unexecuted.

Do not inspect unrelated personal files or modify the existing BioCLIP project. Do not clone an unrelated starter project or download opaque setup scripts.

## 2. Use a small project structure

Create a layout along these lines, adapting it only when an existing project structure makes that necessary:

```text
Plan/
  Step1.md
include/
  net_io.h
src/
  server.c
  client.c
  net_io.c
tests/
  test_network.py
docs/
  architecture.md
  protocol.md
  test-results.md
  ai-usage-log.md
  ai-interactions/
Makefile
README.md
.gitignore
```

Generated binaries and object files belong in ignored build directories. Avoid generated files beside source files. Keep the architecture proportional to this milestone: a shared framing/I/O module plus a client and server is sufficient.

## 3. Define a precise initial wire protocol

Write `docs/protocol.md` before or alongside implementation. Use the same framing in both directions:

```text
4-byte unsigned payload length in network byte order
followed by exactly that many payload bytes
```

Requirements:

- Valid payload length is 1 through 4096 bytes inclusive. A shared constant defines the maximum.
- The client takes a non-empty text message from its command line and sends its bytes without a trailing C null terminator. Lengths count bytes, not characters.
- The server echoes a valid payload exactly, preserving all bytes. The framing helpers treat payloads as bytes and do not depend on null termination or separators.
- For this milestone, one connection carries exactly one request and one response; the connection then closes. Additional pipelined requests are outside the protocol and need not be processed.
- Encode and decode lengths using `htonl()` and `ntohl()`, not host-memory layout assumptions.
- An invalid length, truncated frame, timeout, or other protocol/I/O error causes that connection to close. The server logs a short reason and returns to accepting clients. There is no structured error-response frame in this initial protocol.
- Detect clean EOF before a frame separately from EOF halfway through a header or body.
- Do not allocate memory based on an unchecked network length. A bounded buffer is sufficient.
- Do not treat arbitrary payload bytes as a format string. If payloads are logged, use bounded, escaped output or log only their sizes.

Explain why a single `send()` or `recv()` is insufficient: TCP can fragment a frame across receives or combine data into one receive. The protocol must work independently of those boundaries. Future email commands can be carried inside framed payloads; do not implement that command language now.

## 4. Implement robust shared I/O

Provide small, documented helpers for complete sends, exact-length receives, and frame send/receive. Design explicit return values that distinguish success, clean EOF, truncated input, invalid length, timeout, and other errors where relevant.

- Loop until the requested number of bytes has been transferred or a terminal condition occurs.
- Track progress with appropriate byte counts and types; handle `ssize_t` return values before converting them to unsigned types.
- Retry `EINTR` only when shutdown has not been requested.
- Handle `recv()` returning zero without an infinite loop.
- Handle `send()` failure and unexpected zero progress without an infinite loop.
- Prevent `SIGPIPE` from terminating either process when a peer disconnects. Document the selected Linux/POSIX approach.
- Configure finite receive/send waiting on accepted sockets and the client socket. A five-second inactivity timeout is suitable for the local demonstration. Explain that per-operation or inactivity timeouts are not a strict total-frame deadline; do not claim resistance to every slow-client attack.
- Keep cleanup paths obvious. Every socket must close exactly once on each relevant path.
- Avoid unnecessary heap allocation, global mutable state, generic frameworks, and cryptic macro abstractions.

If connection establishment relies on the operating system's default timeout, document that separately. Do not imply that receive/send timeouts also bound `connect()`.

## 5. Implement the server

Use a clear interface such as:

```sh
./build/server --bind 127.0.0.1 --port 5050
```

- Default to loopback to keep initial testing local. Allow an explicit valid IPv4 bind address and configurable port.
- Validate numeric ports fully: reject empty, negative, trailing-junk, zero, and values above 65535. Do not use unchecked `atoi()`.
- Create an IPv4 TCP socket, configure `SO_REUSEADDR`, bind, and listen.
- Check the return values of every relevant socket/setup function.
- Print the listening address and port after successful startup. Flush readiness output so automated tests can detect it.
- Repeatedly accept connections and call a dedicated `handle_client()` function.
- Receive a valid frame, echo it, close that client's socket, and continue accepting.
- Treat client-specific failures as recoverable. Treat startup failures such as a port already in use as clear errors with a nonzero exit status.
- Handle Ctrl+C and termination with `sigaction()` and minimal, async-signal-safe handlers, such as setting a `volatile sig_atomic_t` shutdown flag. Ensure a server blocked in `accept()` can leave on shutdown; account for interrupted I/O and signal restart behaviour.
- Avoid unsafe logging or allocation inside signal handlers. Release resources from normal control flow.
- Document shutdown timing when a client is being handled, and verify that shutdown completes within the stated bound.

Explain the lifecycle: `socket -> setsockopt -> bind -> listen -> accept -> receive/send -> close client -> accept again`, followed by closing the listening socket at shutdown. Explain that the accepted socket and listening socket have different purposes.

## 6. Implement the client

Use a clear interface such as:

```sh
./build/client --host 127.0.0.1 --port 5050 --message "Hello from the client"
```

- Validate the IPv4 address, port, message length, and command-line structure before use.
- Numeric IPv4 support is enough; hostname resolution and IPv6 are outside this milestone. State this clearly.
- Connect, send one frame, read one response frame, and compare its length and bytes with the original message.
- Print a clear success message only after the response matches.
- Return nonzero for invalid arguments, connection failure, timeout, malformed response, or mismatch.
- Close the socket on success and failure.
- Provide useful `--help` output for both executables.

Explain the client lifecycle: `socket -> connect -> send -> receive -> close`.

## 7. Set up builds and Git

Create a Makefile with these targets:

- `all`: build server and client.
- `debug`: build with debug symbols and useful warnings.
- `sanitize`: build with AddressSanitizer and UndefinedBehaviorSanitizer where supported.
- `test`: run the integration tests against the intended build.
- `clean`: remove only generated build outputs inside this project.

Use C11 and appropriate flags such as `-Wall -Wextra -Wpedantic` and `-g` in debug builds. Include the appropriate POSIX feature-test macro if needed. Do not hide warnings or disable sanitizer checks to obtain a passing result.

Keep normal, debug, and sanitizer build outputs separate, or otherwise guarantee flag changes force correct recompilation. Ensure tests can explicitly select sanitizer binaries; do not assume sanitizers were exercised merely because that build target succeeded.

Initialise Git only if appropriate after the repository inspection. Add a conservative `.gitignore` for build outputs, temporary files, core dumps, and editor/OS clutter. Do not ignore source, documentation, test definitions, or the AI log. Do not modify global Git configuration, invent commit identity, push to a remote, or create commits unless separately requested.

## 8. Verify behaviour with meaningful tests

Use Python 3 standard-library integration tests to launch real server/client processes and raw TCP peers. Do not use mocks for the principal networking checks. No third-party Python test package is required.

The harness must use bounded waits, capture stdout/stderr and exit statuses, clean up only its own child processes in `finally` blocks, and avoid fixed startup sleeps as the sole readiness check. Allow a test-port override and handle/report occupied ports clearly. Do not kill unrelated processes to free a port.

Cover at least:

1. A clean build and a valid client/server message exchange.
2. Multiple sequential client invocations without restarting the server.
3. A header and payload deliberately sent in small fragments.
4. One complete frame sent as a single buffer.
5. A payload at the 4096-byte limit and invalid zero/oversized frame lengths.
6. A peer closing before sending anything, halfway through the header, and halfway through the body.
7. A peer disconnecting before receiving a response; the server remains alive. Account for timing variability rather than asserting one particular socket error.
8. Invalid address/port arguments and an unavailable server.
9. A second server failing clearly when the bind address/port is already in use.
10. A stalled client reaching the documented inactivity timeout; a subsequent healthy client succeeds.
11. A controlled peer returning a mismatched or malformed response; the client exits nonzero.
12. Server shutdown while idle and while handling a stalled connection, within a documented bound.

After malformed-client tests, perform a valid exchange to prove recovery; checking only that the process exists is insufficient. Run appropriate tests against sanitizer binaries when the environment supports them. Record failures honestly and fix defects rather than weakening assertions.

Do not test three simultaneous successful clients as a milestone requirement: this implementation is sequential. Document that concurrency is still pending.

## 9. Make the result teachable

The README should include:

- Milestone scope, implemented features, and pending assignment requirements.
- Verified environment/tool versions and prerequisites.
- Exact setup, build, run, test, sanitizer, and cleanup commands, labelled by shell/environment.
- A two-terminal demonstration using the real executable names and paths.
- How to stop the server, what successful output looks like, and common failures such as connection refused or address already in use.
- Protocol and architecture document links.
- Known limits: sequential service, bounded payloads, IPv4 only, no authentication/storage/encryption, and timeout semantics.

In `docs/architecture.md`, explain the purpose of each source file, the two socket lifecycles, accepted versus listening sockets, network byte order, framing, partial I/O, disconnect handling, and signal handling. Include a short client/server interaction diagram if helpful. Explain why persistent application state and thread synchronization are not needed yet.

### Code readability and comments

Apply these requirements to all code written for this milestone, including test code:

- Use short, meaningful, easily readable variable and function names. Prefer names such as `port`, `sock`, `client_fd`, `msg_len`, and `bytes_sent`. Avoid cryptic abbreviations and single-letter names except conventional short loop indices. Keep names long enough to make their purpose clear.
- Use consistent naming, proper spacing, and four spaces per indentation level in C and Python. Use real tab characters for Makefile recipes, as Make requires.
- Keep functions focused, control flow straightforward, and expressions easy to interpret. Split complicated expressions into clear steps. Prefer readable code over clever or compressed code.
- Separate logical sections with blank lines. Add a short explanatory comment for each logical section, such as argument validation, socket setup, accepting a client, receiving a frame, sending a response, and cleanup.
- Keep comments small, direct, precise, and accurate. Explain the section's purpose or a non-obvious decision without repeating every statement. Do not comment every obvious line.
- Document shared helper functions with a brief description of their purpose, inputs, return values, and important error behaviour. Explain ownership or cleanup responsibilities where relevant.
- Update comments whenever behaviour changes. Never describe intended behaviour as though it is already implemented.
- Review the finished code from the student's perspective: the student should be able to follow each section and explain its variables, decisions, and error paths during the viva.

Include short viva questions about the actual code, such as why a receive loop is needed and why closing an accepted socket does not stop the server. Avoid generic documentation that disagrees with the implementation.

## 10. Maintain an honest AI usage log

For each significant interaction, record the assignment's required fields:

- Date
- Task
- AI Tool
- Prompt/Question
- What Was Used
- Student Verification

Add an entry ID and optional affected files, evidence paths, related commit, rejected suggestions, and outstanding questions. Use the actual date and known tool/model information; do not guess a model version.

Save a faithful copy of this implementation prompt or a clearly labelled summary/reference under `docs/ai-interactions/`. Distinguish this prompt's preparation from the later implementation interaction. Only record earlier planning conversations if their actual content is available; do not invent conversation history.

Crucially, separate:

1. **AI-performed checks:** commands actually executed and observed outcomes.
2. **Student verification:** personal review, explanation, or experiments actually performed by the student.

Mark student verification `Pending student review` until the student confirms their own work. Automated passing tests do not prove student understanding. Never fabricate test outcomes, screenshots, prompts, timestamps, personal observations, or claims of independent authorship.

Write `docs/test-results.md` with the execution environment, actual commands, test outcomes, failures, and remaining unexecuted checks. Label illustrative command output as illustrative. If tests cannot run, report `Not run` and the precise blocker, not `Passed`.

## Definition of done

- [ ] Existing files and this prompt are preserved.
- [ ] Server/client and shared framing helpers are implemented in C.
- [ ] The intended Linux build is reproducible through Make.
- [ ] Invalid inputs and network errors have clear, bounded handling as documented.
- [ ] A successful client disconnect leaves the server ready for the next client.
- [ ] Malformed and interrupted clients do not terminate the server.
- [ ] Integration tests exercise actual network behaviour and clean up their processes.
- [ ] Build/test results are recorded truthfully; environmental blockers are explicit.
- [ ] README, architecture, protocol, and AI log agree with the actual implementation.
- [ ] All code uses short, meaningful names, consistent spacing and indentation, and concise, accurate comments explaining each logical section.
- [ ] Student verification remains pending unless the student has actually confirmed it.
- [ ] No later email features or concurrency claims are presented as completed.

End with a concise report stating what was implemented, the exact build/test results, anything blocked, known limitations, and a short exercise the student can perform to demonstrate understanding. Do not claim the software is failproof or production ready.
