#ifndef NET_IO_H
#define NET_IO_H

#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_PAYLOAD 4096U
#define IO_TIMEOUT 5

/* Status values distinguish a normal close from a broken frame. */
enum net_status {
    NET_OK,
    NET_EOF,
    NET_TRUNCATED,
    NET_INVALID,
    NET_TIMEOUT,
    NET_STOPPED,
    NET_ERROR
};

/* Parse numeric IPv4/port text into addr; return 0 or -1 for invalid input. */
int parse_addr(const char *host, const char *port, struct sockaddr_in *addr);

/* Set five-second send/receive timeouts; return 0 or -1 with errno set. */
int set_timeouts(int sock);

/* Transfer len bytes. Optional stop cancels I/O; neither helper closes sock.
 * NET_ERROR preserves errno. A short receive reports EOF or truncation. */
enum net_status send_all(int sock, const void *buf, size_t len,
                         const volatile sig_atomic_t *stop);
enum net_status recv_exact(int sock, void *buf, size_t len,
                           const volatile sig_atomic_t *stop);

/* Transfer a length-prefixed frame; payload length must be 1..MAX_PAYLOAD.
 * recv_frame needs a MAX_PAYLOAD-byte buffer; len is zero unless successful.
 * Callers own buffers and sockets; statuses follow the byte helpers above. */
enum net_status send_frame(int sock, const void *buf, uint32_t len,
                           const volatile sig_atomic_t *stop);
enum net_status recv_frame(int sock, void *buf, uint32_t *len,
                           const volatile sig_atomic_t *stop);

/* Return a fixed, printable description of an I/O result. */
const char *net_status_name(enum net_status status);

#endif
