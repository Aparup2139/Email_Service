#include "net_io.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>

int parse_addr(const char *host, const char *port, struct sockaddr_in *addr)
{
    /* Require digits only, then check conversion and the TCP port range. */
    if (*port == '\0') {
        return -1;
    }
    for (const char *digit = port; *digit != '\0'; digit++) {
        if (*digit < '0' || *digit > '9') {
            return -1;
        }
    }
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(port, &end, 10);
    if (errno != 0 || *end != '\0' || value == 0 || value > 65535) {
        return -1;
    }

    /* Store addresses and ports in the format expected by socket APIs. */
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons((uint16_t)value);
    return inet_pton(AF_INET, host, &addr->sin_addr) == 1 ? 0 : -1;
}

int set_timeouts(int sock)
{
    struct timeval wait = { .tv_sec = IO_TIMEOUT, .tv_usec = 0 };

    /* Both directions have finite blocking waits after connection setup. */
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait)) < 0) {
        return -1;
    }
    return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &wait, sizeof(wait));
}

static enum net_status io_error(void)
{
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return NET_TIMEOUT;
    }
    return NET_ERROR;
}

enum net_status send_all(int sock, const void *buf, size_t len,
                         const volatile sig_atomic_t *stop)
{
    const unsigned char *bytes = buf;
    size_t sent = 0;

    /* Advance only by the number of bytes actually sent. */
    while (sent < len) {
        if (stop != NULL && *stop) {
            return NET_STOPPED;
        }
        ssize_t count = send(sock, bytes + sent, len - sent, MSG_NOSIGNAL);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return io_error();
        }
        if (count == 0) {
            errno = EIO;
            return NET_ERROR;
        }
        sent += (size_t)count;
    }
    return NET_OK;
}

enum net_status recv_exact(int sock, void *buf, size_t len,
                           const volatile sig_atomic_t *stop)
{
    unsigned char *bytes = buf;
    size_t received = 0;

    /* TCP may deliver a requested byte range in several reads. */
    while (received < len) {
        if (stop != NULL && *stop) {
            return NET_STOPPED;
        }
        ssize_t count = recv(sock, bytes + received, len - received, 0);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return io_error();
        }
        if (count == 0) {
            return received == 0 ? NET_EOF : NET_TRUNCATED;
        }
        received += (size_t)count;
    }
    return NET_OK;
}

enum net_status send_frame(int sock, const void *buf, uint32_t len,
                           const volatile sig_atomic_t *stop)
{
    /* Validate before emitting either part of a frame. */
    if (len == 0 || len > MAX_PAYLOAD) {
        return NET_INVALID;
    }
    uint32_t header = htonl(len);
    enum net_status status = send_all(sock, &header, sizeof(header), stop);
    if (status != NET_OK) {
        return status;
    }
    return send_all(sock, buf, len, stop);
}

enum net_status recv_frame(int sock, void *buf, uint32_t *len,
                           const volatile sig_atomic_t *stop)
{
    /* Read and validate the header before touching the payload buffer. */
    uint32_t header = 0;
    *len = 0;
    enum net_status status = recv_exact(sock, &header, sizeof(header), stop);
    if (status != NET_OK) {
        return status;
    }
    uint32_t size = ntohl(header);
    if (size == 0 || size > MAX_PAYLOAD) {
        return NET_INVALID;
    }

    /* EOF after a valid header always means a truncated frame. */
    status = recv_exact(sock, buf, size, stop);
    if (status == NET_EOF) {
        return NET_TRUNCATED;
    }
    if (status == NET_OK) {
        *len = size;
    }
    return status;
}

const char *net_status_name(enum net_status status)
{
    switch (status) {
    case NET_OK: return "success";
    case NET_EOF: return "peer closed before a frame";
    case NET_TRUNCATED: return "truncated frame";
    case NET_INVALID: return "invalid frame length";
    case NET_TIMEOUT: return "I/O timeout";
    case NET_STOPPED: return "shutdown requested";
    case NET_ERROR: return "socket error";
    }
    return "unknown status";
}
