# Step 1 TCP echo protocol

This milestone transports one bounded request and its identical response per connection. It does not implement email commands or a standard mail protocol.

## Frame layout

| Bytes | Meaning |
|---|---|
| 0 through 3 | Unsigned 32-bit payload length, network byte order (big-endian) |
| 4 onward | Exactly that many payload bytes |

The valid length is 1 through 4096 inclusive. For example, the ASCII payload `Hi` is represented in hexadecimal as `00 00 00 02 48 69`. No null terminator, newline, or extra separator is appended. Length counts bytes, not Unicode characters. C converts the header with `htonl()`/`ntohl()`; Python tests use `struct.pack("!I", length)`.

TCP is a byte stream, not a packet-per-request interface. A header or payload can be split over many receives, and a header plus payload can arrive together. Shared byte-transfer loops consume exactly the required amount. No receive relies on a null terminator.

## Exchange and connection ownership

1. Client connects using numeric IPv4 and TCP.
2. Client sends a complete request frame.
3. Server validates the declared length before receiving into a fixed-size buffer.
4. Server echoes the exact length and payload bytes.
5. Server closes the accepted socket. Client validates the complete reply and closes its socket.
6. Server returns to accepting connections on its separate listening socket.

Client success requires both equal lengths and byte-for-byte equality. Pipelining extra requests on one connection is outside the protocol; no additional request is processed. The receiver buffers are bounded to 4096 bytes; an untrusted declared length never controls an allocation.

## Input and errors

Payloads are arbitrary bytes; delimiters, line breaks, null bytes, and non-ASCII bytes have no special meaning to framing. The CLI passes a nonempty argument's bytes without its C null terminator, so embedded null bytes cannot be supplied from that interface. The server logs sizes and statuses instead of interpreting payloads as strings.

| Condition | Result |
|---|---|
| EOF before any header byte | `NET_EOF`, normal peer close before a frame |
| EOF during a header or after a valid header but before a complete body | `NET_TRUNCATED` |
| Header length 0 or above 4096 | `NET_INVALID` |
| Receive/send wait expires | `NET_TIMEOUT` |
| Shutdown flag observed | `NET_STOPPED` |
| Other transfer error | `NET_ERROR`, with errno retained |
| Complete frame | `NET_OK` |

The server logs the result and closes the connection for any failed exchange. There is no structured error reply at this stage. A malformed response, EOF, invalid length, mismatch, timeout, or other I/O failure makes the client exit nonzero. A successful exchange exits zero.

`recv_exact()` returns `NET_EOF` if it receives no bytes of the requested range. `recv_frame()` converts that status to truncation when the header has already declared a body. This distinction separates an ordinary unused connection from an incomplete request.

## Time limits and interruption

Accepted sockets and connected client sockets use five-second `SO_RCVTIMEO` and `SO_SNDTIMEO` waits. They are per-operation/inactivity limits, not a total-frame deadline. Continuous partial progress can prolong an exchange. Connection establishment uses the operating system's default timeout, because the client sets I/O timeouts after connecting.

Transfer loops retry interrupted calls unless the next iteration observes shutdown. Linux `MSG_NOSIGNAL` makes sends return an error rather than terminating the process with SIGPIPE. No application protocol version negotiation, authentication, encryption, persistent state, or concurrent handling is included yet.
