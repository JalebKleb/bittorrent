#include "network.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static char last_error[256] = "no error";

static void set_last_error(const char *context)
{
    snprintf(last_error, sizeof(last_error), "%s: %s", context, strerror(errno));
}

const char *network_last_error(void)
{
    return last_error;
}

static int set_reuseaddr(int fd)
{
    int yes = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

int network_listen(const char *port, int backlog)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp = NULL;
    int listen_fd = -1;
    int gai_status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    gai_status = getaddrinfo(NULL, port, &hints, &result);
    if (gai_status != 0) {
        snprintf(last_error, sizeof(last_error), "getaddrinfo: %s", gai_strerror(gai_status));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_fd < 0) {
            continue;
        }

        if (set_reuseaddr(listen_fd) != 0) {
            set_last_error("setsockopt(SO_REUSEADDR)");
            network_close(listen_fd);
            listen_fd = -1;
            continue;
        }

        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            if (listen(listen_fd, backlog) == 0) {
                break;
            }
            set_last_error("listen");
        } else {
            set_last_error("bind");
        }

        network_close(listen_fd);
        listen_fd = -1;
    }

    freeaddrinfo(result);

    if (listen_fd < 0 && strcmp(last_error, "no error") == 0) {
        snprintf(last_error, sizeof(last_error), "could not create listening socket");
    }

    return listen_fd;
}

int network_accept(int listen_fd,
                   char *peer_host,
                   size_t peer_host_len,
                   char *peer_port,
                   size_t peer_port_len)
{
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    int client_fd;

    client_fd = accept(listen_fd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    if (client_fd < 0) {
        set_last_error("accept");
        return -1;
    }

    if (peer_host != NULL && peer_host_len > 0 && peer_port != NULL && peer_port_len > 0) {
        int status = getnameinfo((struct sockaddr *)&peer_addr,
                                 peer_addr_len,
                                 peer_host,
                                 (socklen_t)peer_host_len,
                                 peer_port,
                                 (socklen_t)peer_port_len,
                                 NI_NUMERICHOST | NI_NUMERICSERV);
        if (status != 0) {
            snprintf(peer_host, peer_host_len, "unknown");
            snprintf(peer_port, peer_port_len, "0");
        }
    }

    return client_fd;
}

int network_connect(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp = NULL;
    int sock_fd = -1;
    int gai_status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    gai_status = getaddrinfo(host, port, &hints, &result);
    if (gai_status != 0) {
        snprintf(last_error, sizeof(last_error), "getaddrinfo: %s", gai_strerror(gai_status));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd < 0) {
            continue;
        }

        if (connect(sock_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        set_last_error("connect");
        network_close(sock_fd);
        sock_fd = -1;
    }

    freeaddrinfo(result);

    if (sock_fd < 0 && strcmp(last_error, "no error") == 0) {
        snprintf(last_error, sizeof(last_error), "could not connect socket");
    }

    return sock_fd;
}

ssize_t network_send_all(int fd, const void *buffer, size_t length)
{
    const unsigned char *cursor = (const unsigned char *)buffer;
    size_t total = 0;

    while (total < length) {
        ssize_t sent = send(fd, cursor + total, length - total, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_last_error("send");
            return -1;
        }
        if (sent == 0) {
            break;
        }
        total += (size_t)sent;
    }

    return (ssize_t)total;
}

ssize_t network_recv_all(int fd, void *buffer, size_t length)
{
    unsigned char *cursor = (unsigned char *)buffer;
    size_t total = 0;

    while (total < length) {
        ssize_t received = recv(fd, cursor + total, length - total, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_last_error("recv");
            return -1;
        }
        if (received == 0) {
            break;
        }
        total += (size_t)received;
    }

    return (ssize_t)total;
}

void network_close(int fd)
{
    if (fd >= 0) {
        while (close(fd) != 0 && errno == EINTR) {
        }
    }
}
