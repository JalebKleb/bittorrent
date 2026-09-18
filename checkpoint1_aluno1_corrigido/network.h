#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <sys/types.h>

#define NETWORK_DEFAULT_BACKLOG 16

int network_listen(const char *port, int backlog);
int network_accept(int listen_fd,
                   char *peer_host,
                   size_t peer_host_len,
                   char *peer_port,
                   size_t peer_port_len);
int network_connect(const char *host, const char *port);

ssize_t network_send_all(int fd, const void *buffer, size_t length);
ssize_t network_recv_all(int fd, void *buffer, size_t length);

void network_close(int fd);
const char *network_last_error(void);

#endif
