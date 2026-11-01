#include "net_io.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile sig_atomic_t stop = 0;

static void request_stop(int signum)
{
    /* Signal handlers only change the shutdown flag. */
    (void)signum;
    stop = 1;
}

static void usage(FILE *out)
{
    fprintf(out, "Usage: server [--bind IPv4] [--port 1..65535]\n"
                 "Defaults: --bind 127.0.0.1 --port 5050\n");
}

static int read_args(int argc, char **argv, struct sockaddr_in *addr)
{
    const char *host = "127.0.0.1";
    const char *port = "5050";
    int seen_host = 0;
    int seen_port = 0;
    const struct option opts[] = {
        { "bind", required_argument, NULL, 'b' },
        { "port", required_argument, NULL, 'p' },
        { "help", no_argument, NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    /* Reject duplicate options, missing values, and extra positional input. */
    int opt;
    opterr = 0;
    while ((opt = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (opt) {
        case 'b':
            if (seen_host++) { return -1; }
            host = optarg;
            break;
        case 'p':
            if (seen_port++) { return -1; }
            port = optarg;
            break;
        case 'h':
            if (argc != 2) { return -1; }
            usage(stdout);
            return 1;
        default: return -1;
        }
    }
    if (optind != argc) {
        return -1;
    }
    return parse_addr(host, port, addr);
}

static void handle_client(int client_fd)
{
    unsigned char msg[MAX_PAYLOAD];
    uint32_t len = 0;

    /* Limit inactivity before reading a single request. */
    if (set_timeouts(client_fd) < 0) {
        perror("client timeouts");
        return;
    }
    enum net_status status = recv_frame(client_fd, msg, &len, &stop);
    if (status == NET_OK) {
        status = send_frame(client_fd, msg, len, &stop);
    }

    /* Log sizes and statuses, never untrusted message contents. */
    if (status == NET_OK) {
        printf("Echoed %u bytes\n", (unsigned int)len);
        fflush(stdout);
    } else {
        int saved_errno = errno;
        fprintf(stderr, "Client: %s", net_status_name(status));
        if (status == NET_ERROR) {
            fprintf(stderr, ": %s", strerror(saved_errno));
        }
        fputc('\n', stderr);
    }
}

int main(int argc, char **argv)
{
    struct sockaddr_in addr;
    int args = read_args(argc, argv, &addr);
    if (args == 1) {
        return 0;
    }
    if (args < 0) {
        fprintf(stderr, "Invalid server arguments or IPv4 address/port.\n");
        usage(stderr);
        return 1;
    }

    /* Do not restart blocked calls automatically after a stop signal. */
    struct sigaction action = { 0 };
    action.sa_handler = request_stop;
    if (sigemptyset(&action.sa_mask) < 0 ||
        sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0) {
        perror("signal setup");
        return 1;
    }

    /* The listening socket stays open across individual connections. */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    int result = 1;
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("SO_REUSEADDR");
        goto cleanup;
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        goto cleanup;
    }
    if (listen(sock, 16) < 0) {
        perror("listen");
        goto cleanup;
    }
    /* A vanished pending connection must not leave accept blocked. */
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("listener nonblocking mode");
        goto cleanup;
    }
    char host[16];
    /* inet_ntop avoids logging unvalidated command-line text. */
    if (inet_ntop(AF_INET, &addr.sin_addr, host, sizeof(host)) == NULL) {
        perror("inet_ntop");
        goto cleanup;
    }
    printf("Listening on %s:%u\n", host, (unsigned int)ntohs(addr.sin_port));
    fflush(stdout);

    /* Poll bounds idle shutdown even if a signal arrives before the wait. */
    struct pollfd ready = { .fd = sock, .events = POLLIN, .revents = 0 };
    while (!stop) {
        int count = poll(&ready, 1, 250);
        if (count < 0) {
            if (errno == EINTR) { continue; }
            perror("poll");
            goto cleanup;
        }
        if (stop || count == 0) { continue; }
        if (!(ready.revents & POLLIN)) {
            fprintf(stderr, "Listening socket failed.\n");
            goto cleanup;
        }

        /* Each accepted socket belongs to this iteration only. */
        int client_fd = accept(sock, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR || errno == ECONNABORTED ||
                errno == EAGAIN || errno == EWOULDBLOCK) { continue; }
            perror("accept");
            goto cleanup;
        }
        printf("Client connected.\n");
        fflush(stdout);
        handle_client(client_fd);
        if (close(client_fd) < 0) {
            perror("close client");
        }
    }
    result = 0;
    printf("Server stopped.\n");

cleanup:
    /* Never retry close: the descriptor may already have been released. */
    if (close(sock) < 0) {
        perror("close listener");
        result = 1;
    }
    return result;
}
