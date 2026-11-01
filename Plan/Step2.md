# Step 2 Plan Protocol User Storage and Accounts

## Purpose and current status

This is the implementation prompt for the next milestone in `D:\Emai_service_project`. When asked to implement Step 2, complete the work below using WSL distribution `Ubuntu-24.04` and Linux path `/mnt/d/Emai_service_project`.

The current milestone provides a sequential, framed TCP echo service. The student supplied passing normal/debug/sanitizer test output and a manual server demonstration. That evidence does not establish code understanding. Do not rewrite the working foundation or claim account features already exist.

**Target outcome:** a student can register an account, restart the server without losing that account, log in, identify the authenticated user, log out, and receive clear errors for invalid requests. Existing echo behaviour remains available and its regression suite still passes.

This document is a plan only. Preparing it does not authorise implementing Step 2 during the planning task. When implementation is explicitly requested, carry the milestone through code, documentation, genuine tests, and a truthful AI log.

## 1. Inspect and establish the baseline

1. Read applicable `AGENTS.md`, the retained Step 1 plan, current C sources, Makefile, README, protocol documentation, and tests. Inspect Git status; preserve unrelated changes and existing evidence.
2. Confirm the repository, branch, and Ubuntu environment. Do not silently use the Docker WSL distribution, alter global Git configuration, or reset existing history.
3. Run the current normal tests once as a baseline. Record actual failures before changing code. Do not rerun unchanged suites repeatedly without a reason.
4. Write the Step 2 protocol, storage, and session contracts before implementing their handlers. Distinguish assignment requirements from the design decisions proposed here.

## 2. Scope and architecture

Implement registration, login, session lookup, logout, and persistent user records. Keep the server sequential for this milestone. Mail sending, inbox/sent mail, read status, attachments, TLS, and concurrent request handling remain later work.

Keep shared framing unchanged: four-byte network-order length followed by 1..4096 payload bytes. Add an explicit `--mode echo|service` to the server and client, defaulting to `echo` to preserve current commands/tests. Service traffic is never silently interpreted as echo traffic or vice versa. Document both modes and their incompatible payload semantics.

Suggested modules, created only where actual implemented behaviour justifies them:

```text
include/ and src/
  net_io.*       existing framing and byte transfers
  protocol.*     service request parsing, validation, response encoding
  users.*        bounded user records and durable storage
  auth.*         password verification and in-memory sessions
  server.c       startup, mode selection, dispatch, cleanup
  client.c       echo interface or interactive account menu
tests/
  test_network.py       retained echo regression suite
  test_accounts.py      service integration and persistence tests
docs/
  protocol.md           retain Step 1 section and add versioned service spec
  storage-design.md
  test-plan.md
  architecture.md
```

Keep functions focused and interfaces small. Use short meaningful names, four-space C/Python indentation, proper spacing, and blank lines between logical sections. Add small, precise comments explaining each section and non-obvious decisions. Header comments must specify inputs, outputs, ownership, errors, and cleanup. Do not add speculative frameworks or empty mail modules.

## 3. Select and verify two narrowly scoped libraries

Use **Jansson** for JSON parsing/encoding and **libsodium** for password hashing and random tokens, subject to verifying the installed Ubuntu versions and API support. Socket programming and application logic remain C. Explain why each library is preferable to a handwritten JSON parser or cryptographic algorithm.

Inspect `pkg-config` and installed packages first. If missing, document the Ubuntu prerequisite command:

```sh
sudo apt update
sudo apt install libjansson-dev libsodium-dev pkg-config
```

Install only within available permissions; do not launch an elevated installer or alter Windows settings without authorisation. If unavailable, finish independent work and report the blocker. Never silently replace secure password hashing with plaintext, a fast general-purpose hash, or custom cryptography.

Use `pkg-config` flags and fail clearly for missing dependencies. Preserve normal/debug/sanitizer modes and ensure tests select the intended executables. Keep compiler warnings and fatal sanitizer diagnostics enabled. Record the actual installed versions; do not assume the latest online documentation exactly matches Ubuntu's package.

Official references checked while preparing this plan:

- [Jansson API reference](https://jansson.readthedocs.io/en/latest/apiref.html): bounded decoding, duplicate-key rejection, UTF-8 string lengths, and reference ownership.
- [Libsodium password-hashing API](https://libsodium.gitbook.io/doc/password_hashing/default_phf): `crypto_pwhash_str()` and `crypto_pwhash_str_verify()`.
- [Libsodium random generation](https://libsodium.gitbook.io/doc/generating_random_data): `randombytes_buf()`.

## 4. Specify the service protocol

Each service connection still carries exactly one request and one response. The framed payload is one UTF-8 JSON object, with `v` equal to integer `1` and an uppercase `cmd` string. A session belongs to the service, not a particular TCP connection, so an interactive client can reconnect for each menu action without blocking the sequential server while the student is typing.

Commands for this milestone:

| Command | Request fields beyond `v` and `cmd` | Success code and data |
|---|---|---|
| `REGISTER` | `username`, `email`, `password` | `REGISTERED`, public user ID/name/email; no automatic login |
| `LOGIN` | `username`, `password` | `LOGIN_OK`, public user data, bearer token, expiry interval |
| `WHOAMI` | `token` | `AUTHENTICATED`, public user data |
| `LOGOUT` | `token` | `LOGGED_OUT`, token invalidated |

Use this response envelope consistently:

```json
{"v":1,"ok":true,"code":"REGISTERED","data":{"id":1,"username":"alice","email":"alice@iitlab.local"}}
```

```json
{"v":1,"ok":false,"code":"AUTH_FAILED","message":"Invalid username or password"}
```

These are illustrative examples, not test results. Define exact allowed fields/types for every command, and reject missing, unknown, duplicate, null, or incorrectly typed fields. Use `json_loadb()` with the exact frame length and duplicate-key rejection. Require a root object, a supported version, valid UTF-8, no trailing non-whitespace content, and no embedded nulls. Do not enable parser options that accept embedded nulls. Enforce decoded byte limits, not just encoded-frame limits.

Document stable errors including `BAD_REQUEST`, `UNSUPPORTED_VERSION`, `UNKNOWN_COMMAND`, `INVALID_FIELD`, `USERNAME_EXISTS`, `EMAIL_EXISTS`, `AUTH_FAILED`, `UNAUTHENTICATED`, `SESSION_EXPIRED`, `LIMIT_REACHED`, and `STORAGE_ERROR`. A wrong password and a nonexistent username share `AUTH_FAILED`; do not claim this alone prevents timing-based enumeration.

Malformed framing closes the connection as in Step 1. A fully received but invalid JSON/application request gets a small bounded error response where possible, then closes. Never include passwords, verifiers, tokens, raw requests, or internal filesystem paths in diagnostic messages. Check response size before framing; preserve correct Jansson reference ownership on all paths.

## 5. Fix validation and identity rules

Use explicit bounded rules and test their boundaries:

- Username: 3..32 ASCII bytes, lowercase letters/digits/underscore; reject other forms rather than silently changing identity.
- Email: up to 128 ASCII bytes, one nonempty local part and an `iitlab.local` domain for this self-contained service. Specify a simple allowed local-part alphabet; store accepted addresses lowercase and enforce uniqueness on the canonical value. Explain that this is a project rule, not a complete Internet email validator or assignment-mandated domain.
- Password: 8..128 UTF-8 bytes; reject embedded nulls/control characters. Do not trim, lowercase, or silently truncate it. The decoded password is never a username/path/file name.
- User IDs: positive integers, unique and stable across restart; do not reuse IDs. No account deletion is implemented yet.
- Initial limits: at most 1000 users and 128 live sessions, with explicit capacity errors and bounded storage parsing. State any changed limits and why.

Use fixed maximums from shared definitions rather than scattered magic numbers. Validate on the server even when the client has already checked input. IDs or claimed usernames supplied by a client never establish authentication.

## 6. Passwords and sessions

Call and check `sodium_init()` once. Store only libsodium's encoded password verifier, which contains the salt and algorithm parameters. Use its high-level hash/verify APIs and documented interactive resource limits; check allocation/hash failures. Measure hashing time so socket/test timeouts remain realistic. Never log passwords or persist raw requests. Clear temporary password buffers with `sodium_memzero()` where ownership permits, without overstating guarantees about every library copy or swapped memory.

Generate 32 random bytes per login and encode them as a 64-character hexadecimal bearer token. Store session state only in server memory: token, user ID, and expiry. A fixed 30-minute lifetime measured using a monotonic clock is enough; successful requests do not extend it. Bound the table and purge expired entries. Never derive tokens from usernames, timestamps, `rand()`, or process IDs.

Permit multiple sessions per user up to the global limit. Logging out invalidates only the supplied session. Disconnecting a per-request TCP connection does not log out; restarting the server invalidates all tokens while keeping user accounts. Unknown tokens return `UNAUTHENTICATED`; a known expired token may return `SESSION_EXPIRED` before removal. The client returns to the login menu on either.

Keep tokens in client-process memory; do not put them in command-line arguments, source fixtures, disk files, or logs. Exit attempts logout when possible, then clears local state even if the server is unavailable. A bounded best-effort logout failure is reported without inventing success. Plain TCP exposes credentials and bearer tokens to traffic observers, so bind to loopback and document the absence of TLS.

## 7. Design durable user storage

Add a service-mode `--data-dir` option. Use a documented default such as `data/`, ignore generated account/session-related files in Git, and use synthetic test fixtures only. Configure tests with isolated temporary directories, never the student's real account records.

Use a small versioned JSON account file, for example `users.json`, containing `schema_version`, `next_user_id`, and an array of records with ID, username, canonical email, and encoded password hash. Persist no bearer tokens. Use the same JSON library, but impose a separate storage-file limit, such as 2 MiB; the network's 4096-byte frame limit is not a database-file limit.

Startup must distinguish a genuinely new data directory from a corrupt/inaccessible existing file. Validate the schema, record types, IDs, usernames, duplicate identities, verifier format, and monotonic next ID. A corrupt or incompatible file is a visible startup failure, not an excuse to overwrite it with an empty database.

Registration sequence:

1. Validate input, capacity, and uniqueness against the current user set.
2. Create the verifier and prepare a candidate updated set without publishing it as committed state.
3. Create a unique temporary file in the same directory with restrictive permissions; serialize the candidate; check write, flush, sync, and close results.
4. Atomically rename the temporary file over the account file; sync the parent directory where supported.
5. Publish the new in-memory state only consistently with what reached the file; send success only after the defined commit point.

Specify handling of failures before rename versus after rename. A directory-sync failure after rename cannot honestly be described as a guaranteed rollback; reconcile/reload state and report an indeterminate durability failure if necessary. Do not claim perfect crash durability on WSL's Windows-mounted filesystem. Report actual permission and filesystem behaviour; do not claim `0600` provides verified Windows ACL isolation without checking it.

Hold an exclusive server-lifetime lock for the service data directory so two processes cannot overwrite each other's registrations, even if they listen on different ports. Release resources on shutdown. Do not automatically delete unknown temporary files. Future threading will need an explicit mutex around uniqueness checks, ID allocation, session mutation, and storage commits; do not claim that lock exists yet.

## 8. Interactive client and learning flow

Service mode offers Register, Login, Who am I, Logout, and Exit. Echo mode retains `--message` and its existing behaviour. Document exactly which CLI options apply in each mode and reject incompatible combinations.

Prompt locally before opening the request connection, so idle menu time does not occupy the server. Use bounded input handling, reject overlong lines without splitting them into the next answer, and handle EOF cleanly. Hide password input on a terminal, restore terminal settings on every handled exit/error, and support redirected stdin for integration tests without pretending it is secret. Do not accept passwords as process arguments.

Explain each new concept in `docs/architecture.md`: JSON ownership, request dispatch, identity validation, password verifiers, bearer sessions, persistence, and write-commit boundaries. Add short viva questions and a walkthrough of registration/login/logout. Never mark the student's understanding verified merely because this explanation exists.

## 9. Implementation order and completion checks

| Stage | Work | Evidence required |
|---|---|---|
| A | Write protocol/storage/session contracts and dependency setup | Exact examples, limits, errors, and state transitions agree |
| B | Add parsing/response helpers and explicit echo/service modes | Malformed input rejected; original echo suite passes |
| C | Add isolated user storage and password hashing | Registration persists; duplicates and failed writes do not silently lose records |
| D | Add login, session lookup, and logout | Authenticated identity comes only from valid server-issued tokens |
| E | Add interactive service menu | Real terminal flow works; redirected-input integration tests also work |
| F | Run regression/security/persistence tests and update documentation/logs | Actual normal/debug/sanitizer results retained; limits stated honestly |

No code should be committed or pushed by the future implementation agent unless the student authorises it in that interaction. Preserve the current evidence and original prompt snapshots.

## 10. Required tests

Use real C executables and TCP peers with Python standard-library orchestration, bounded waits, readiness detection, and reliable cleanup. Library-specific unit tests may supplement integration tests where controlled failure/clock injection is needed; keep such hooks local to test code, not publicly enabled service features.

- Run every retained Step 1 echo test against the explicit/default echo behaviour.
- Register a valid user, restart the server on the same isolated data directory, and log in successfully.
- Reject duplicate username, canonical duplicate email, invalid field types, missing/unknown fields, invalid boundaries, and excess capacity.
- Inspect stored records to confirm the exact plaintext password is absent; verify the encoded hash using the chosen library. Never print credentials/verifiers into evidence.
- Test wrong password and unknown username with the same public error code.
- Test valid/unknown/expired/malformed tokens, two independent sessions, logout invalidation, and restart invalidation. Exercise expiry with a controlled test clock rather than waiting 30 minutes or exposing a production bypass.
- Reject duplicate JSON keys, embedded nulls, invalid UTF-8, wrong versions, non-object roots, trailing data, unknown commands, and malformed frames; then prove recovery with a valid request.
- Test fragmented requests and correct response framing/limits.
- Exercise unwritable storage as the unprivileged Ubuntu user, corrupt/truncated/unsupported storage files, and two servers sharing one data directory. Verify existing valid records remain intact. Clearly separate simulated write failures from actual filesystem failures.
- Test menu input bounds, EOF, state after connection failure, and logout failure. Acknowledge any terminal-restoration paths requiring manual verification.
- Run normal, debug, and ASan/UBSan account suites plus the echo regression suites. Record actual library versions, commands, exit codes, and diagnostics. Do not weaken sanitizers or assertions to make tests pass.

## 11. Documentation AI log and definition of done

Update README, protocol, architecture, storage design, test plan/results, and the AI usage log. Save significant prompts or labelled summaries. Separate AI-performed tests from student execution and student understanding; never fabricate screenshots or experimental observations.

- [ ] Registration/login/session/logout work through the C service client and server.
- [ ] Existing echo commands and all framing regression tests remain usable.
- [ ] Accounts survive restart; session tokens do not.
- [ ] Identity uniqueness, validation, capacity, and ownership rules are enforced server-side.
- [ ] No plaintext password persistence, token logging, or custom cryptography.
- [ ] Failed/corrupt storage does not silently reset accounts or report unearned success.
- [ ] Test data and generated credentials are excluded from Git.
- [ ] Real WSL tests and sanitizer runs are recorded accurately.
- [ ] Code remains readable with short meaningful names and concise section comments.
- [ ] Sequential service, no TLS, filesystem durability limits, and deferred mail/concurrency work are explicit.
- [ ] Student learning checks remain pending until personally demonstrated.

End implementation with a concise report of changes, test results, blockers, remaining limits, and a practical student exercise: register two users, restart, log in as each, verify identity, log out, and explain where the verifier is stored and why it is not a plaintext password.
