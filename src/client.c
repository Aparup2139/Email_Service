#include "net_io.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void usage(FILE *out)
{
    fprintf(out, "Usage: client [--host IPv4] [--port 1..65535] --message TEXT\n"
                 "Defaults: --host 127.0.0.1 --port 5050\n"
                 "Message length: 1..4096 bytes\n");
}

static int read_args(int argc, char **argv, struct sockaddr_in *addr,
                     const char **msg)
{
    const char *host = "127.0.0.1";
    const char *port = "5050";
    int seen_host = 0;
    int seen_port = 0;
    const struct option opts[] = {
        { "host", required_argument, NULL, 'a' },
        { "port", required_argument, NULL, 'p' },
        { "message", required_argument, NULL, 'm' },
        { "help", no_argument, NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    /* Parse only documented options and reject duplicate values. */
    int opt;
    opterr = 0;
    while ((opt = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (opt) {
        case 'a':
            if (seen_host++) { return -1; }
            host = optarg;
            break;
        case 'p':
            if (seen_port++) { return -1; }
            port = optarg;
            break;
        case 'm':
            if (*msg != NULL) { return -1; }
            *msg = optarg;
            break;
        case 'h':
            if (argc != 2) { return -1; }
            usage(stdout);
            return 1;
        default: return -1;
        }
    }
    if (optind != argc || *msg == NULL || strlen(*msg) == 0 ||
        strlen(*msg) > MAX_PAYLOAD) {
        return -1;
    }
    return parse_addr(host, port, addr);
}

int main(int argc, char **argv)
{
    /* Validate all input before creating a socket. */
    struct sockaddr_in addr;
    const char *msg = NULL;
    int args = read_args(argc, argv, &addr, &msg);
    if (args == 1) { return 0; }
    if (args < 0) {
        fprintf(stderr, "Invalid client arguments, address, port, or message.\n");
        usage(stderr);
        return 1;
    }

    /* Connect uses the OS default connection-establishment timeout. */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    int result = 1;
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        goto cleanup;
    }
    if (set_timeouts(sock) < 0) {
        perror("socket timeouts");
        goto cleanup;
    }

    /* Send one request, then verify the complete echoed response. */
    unsigned char reply[MAX_PAYLOAD];
    uint32_t reply_len = 0;
    uint32_t len = (uint32_t)strlen(msg);
    enum net_status status = send_frame(sock, msg, len, NULL);
    if (status == NET_OK) {
        status = recv_frame(sock, reply, &reply_len, NULL);
    }
    if (status != NET_OK) {
        int saved_errno = errno;
        fprintf(stderr, "Exchange failed: %s", net_status_name(status));
        if (status == NET_ERROR) {
            fprintf(stderr, ": %s", strerror(saved_errno));
        }
        fputc('\n', stderr);
        goto cleanup;
    }
    if (reply_len != len || memcmp(msg, reply, len) != 0) {
        fprintf(stderr, "Exchange failed: response does not match request.\n");
        goto cleanup;
    }
    printf("Success: echoed %u bytes; response matches.\n", (unsigned int)len);
    result = 0;

cleanup:
    /* This function owns the socket on every post-creation path. */
    if (close(sock) < 0) {
        perror("close client");
        result = 1;
    }
    return result;
}
