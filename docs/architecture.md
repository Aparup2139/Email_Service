# Networking foundation and viva guide

The application has two C executables and a small shared module. The server handles one request per connection, sequentially. Both programs use the framing and status definitions in `include/net_io.h`.

## File responsibilities

- `server.c`: validate options, install signal handlers, create the listener, accept clients, invoke `handle_client()`, and close resources.
- `client.c`: validate options/message, connect, exchange a frame, verify the response, and close its socket.
- `net_io.c`: validate addresses/ports, configure timeouts, send/receive byte ranges, and encode/decode frames.
- `test_network.py`: start actual binaries, provide controlled TCP peers, assert behaviour, and clean up its own subprocesses.

Variables have short descriptive names, such as `sock`, `client_fd`, `len`, and `received`. Each major section has a concise comment explaining its purpose. No dynamic allocation is needed for payloads because every payload has a fixed upper bound.

## Socket lifecycle

```text
Client                                      Server
                                            socket
                                            setsockopt(SO_REUSEADDR)
                                            bind -> listen
socket                                      poll listener
connect ----------------------------------> accept -> separate client_fd
send_frame -------------------------------> recv_frame
recv_frame <------------------------------- send_frame
compare reply                               close(client_fd)
close(sock)                                 poll listener again
                                            stop signal -> close(listener)
```

`socket()` creates an endpoint. `bind()` selects the local address/port. `listen()` enables incoming connections and supplies a pending-connection backlog. `accept()` returns a new connected socket; it does not replace or close the listener. `connect()` establishes the client connection. `send()` and `recv()` transfer bytes; a positive return may be smaller than requested. `close()` releases an endpoint.

`SO_REUSEADDR` assists rebinding after a previous server exits; it does not permit two live listeners on the same address/port in this implementation. Only numeric IPv4 addresses are accepted. Ports are checked as decimal digits and converted with `strtoul()` and range/error checks before conversion to 16 bits.

## Transfer and framing decisions

The header is a fixed-width `uint32_t`, encoded in network byte order. `send_all()` and `recv_exact()` track byte progress and handle partial transfers. Signed return values are checked before conversion to `size_t`. Zero receive progress means EOF, not a reason to loop forever. Zero send progress is treated as an error.

`recv_frame()` validates the header before receiving into the caller's bounded buffer. The buffer is bytes, not a C string. `send_frame()` validates outgoing lengths as well. The client compares lengths before calling `memcmp()` so a short response cannot trigger comparison of uninitialised buffer contents. Linux `MSG_NOSIGNAL` prevents a disconnected peer from terminating either executable via SIGPIPE.

Statuses distinguish EOF, truncated frames, invalid lengths, timeout, shutdown, and OS errors. The executable logs the reason and owns its sockets; shared helpers never close them. Socket-error errno is saved before logging. Descriptors are closed once, and `close()` is not retried after an error.

## Shutdown and blocking behaviour

The server's signal handler for SIGINT/SIGTERM only sets a `volatile sig_atomic_t` flag. It does not print, allocate, or close resources. Calls are installed without `SA_RESTART`, so interrupted operations return to code that can inspect the flag.

The listening socket is nonblocking. `poll()` waits up to 250 ms, then the main loop checks shutdown. Nonblocking `accept()` avoids getting stuck if a pending connection disappears between readiness and acceptance. On Linux, the accepted socket does not inherit the listener's nonblocking flag; it uses blocking I/O with explicit receive/send timeouts.

Accepted connections receive five-second send/receive timeouts. Transfer loops check shutdown before each call and after interruptions via the next iteration. A signal delivered just before a call can leave it waiting until that call's timeout. The test bound is seven seconds, including scheduling overhead; it is not a hard real-time guarantee. Client `connect()` retains the OS default timeout.

Transient interrupted/aborted accepts return to the loop. Other listener failures are reported and end the server; malformed client frames end only that connection. An exhausted process file-descriptor limit is not handled with an elaborate recovery policy in this first milestone.

## Later concurrency and storage work

There are no users or messages to retain yet, and there are no worker threads. Therefore no persistent records, mail IDs, mutexes, or database are needed. `handle_client()` is isolated for a later concurrency change, but adding threads will also require revisiting shutdown, ownership, limits, and any shared storage. The backlog alone is not concurrency.

## Questions to practise

1. Why do we keep the listener open after closing `client_fd`? They are separate sockets; only the accepted one represents that exchange.
2. Why can one `recv()` be insufficient? TCP can provide only part of the requested byte range.
3. Why encode the header instead of sending host-endian integers? Machines can use different byte orders.
4. Why check length before receiving the body? Otherwise an untrusted peer could exceed the buffer.
5. Why does EOF during a body mean truncation? The header promised more bytes than arrived.
6. Why use `MSG_NOSIGNAL`? A peer can disconnect before a send, and that must produce an error rather than kill the process.
7. Why poll a nonblocking listener? Readiness plus a bounded wait supports predictable idle shutdown without an accept race.
8. Why is the server still sequential? It completes `handle_client()` before accepting and processing the next connection.
9. Does a five-second timeout guarantee a five-second total exchange? No; separate calls and ongoing progress can extend it.
10. Where are emails stored? Nowhere yet; this milestone only exchanges bytes.

Student understanding has not been verified by these notes or automated tests. Practise explaining the actual functions and modifying a small part before marking student verification complete.
