//
// Created by Kok on 8/23/26.
//

#include "socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "http.h"
#include "config.h"

static void _poll_socket(socket_handle_t *hsocket);
static void _check_for_expired_conn(socket_handle_t *hsocket);

static void _add_connection(int fd, bool is_listen_fd, socket_handle_t *hsocket);
static void _close_flagged_conns(socket_handle_t *hsocket);
static void _close_conn(socket_conn_t *conn, socket_handle_t *hsocket);

int socket_init(socket_handle_t *hsocket) {
    struct in_addr in_addr;
    if (inet_aton(hsocket->config.address, &in_addr) == 0) {
        errno = EINVAL;
        return 1;
    }

    struct sockaddr_in sock_addr = {.sin_family = AF_INET, .sin_port = htons(hsocket->config.port), .sin_addr = {.s_addr = in_addr.s_addr}};

    // Initialize listening socket
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        return 1;
    }

    // Bypass lingering wait time from previous sockets
    int on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // Make sure accept() doesn't block
    int fd_flags = fcntl(listen_fd, F_GETFL, 0);
    if (fd_flags < 0) {
        close(listen_fd);
        return 1;
    }

    if (fcntl(listen_fd, F_SETFL, fd_flags | O_NONBLOCK) < 0) return 1;

    // Bind the listening socket to an address
    int ret = bind(listen_fd, (struct sockaddr *) &sock_addr, sizeof(sock_addr));
    if (ret < 0) {
        close(listen_fd);
        return 1;
    }

    // Add the listening socket to the connections linked list
    _add_connection(listen_fd, true, hsocket);

    return 0;
}

int socket_listen(socket_handle_t *hsocket) {
    int ret = listen(hsocket->connections->fd, hsocket->config.max_conn_count);
    if (ret < 0) return 1;

    hsocket->config.event_cb(SOCKET_EVENT_LISTENING, NULL);
    while (true) _poll_socket(hsocket);

    return 0;
}

int socket_write(char *data, size_t len, socket_conn_t *conn) {
    size_t total_sent = 0;
    while (total_sent < len) {
        size_t sent = send(conn->fd, data, len, 0);
        if (sent > 0) {
            total_sent += sent;
            continue;
        }

        if (sent < 0) {
            if (errno == EINTR) continue;
            else if (errno == EAGAIN) {
                // Send buffer is full
                struct pollfd pfd = {.fd = conn->fd, .events = POLLOUT};
                int ret = poll(&pfd, 1, SOCKET_WRITE_TIMEOUT_MS);
                if (ret < 0) {
                    if (errno == EINTR) continue;
                    return 1;
                }
                continue;
            }
            return 1;
        }
    }

    return 0;
}

void socket_close_connection(socket_conn_t *conn) {
    conn->flags |= SOCKET_CONN_FLAG_CLOSING;    // The actual closing happens in the _poll_socket loop
}

void _poll_socket(socket_handle_t *hsocket) {
    fd_set fdset;
    FD_ZERO(&fdset);
    int max_fd = 0;

    // Add all connections to the set
    for (socket_conn_t *conn = hsocket->connections; conn != NULL; conn = conn->next) {
        FD_SET(conn->fd, &fdset);
        if (conn->fd > max_fd) max_fd = conn->fd;
    }

    // Wait for some network event
    struct timeval tv = {0};
    tv.tv_sec = SOCKET_WAIT_TIMEOUT_SECONDS;
    int ret = select(max_fd + 1, &fdset, NULL, NULL, &tv);
    if (ret < 0) {
        perror("socket_error");
        return;
    }

    // Check for TTL
    _check_for_expired_conn(hsocket);

    // Check which connection are ready to be read
    for (socket_conn_t *conn = hsocket->connections; conn != NULL; conn = conn->next) {
        if (FD_ISSET(conn->fd, &fdset)) conn->flags |= SOCKET_CONN_FLAG_READ_READY;
    }

    socket_conn_t *conn = hsocket->connections;
    while (conn != NULL) {
        if ((conn->flags & SOCKET_CONN_FLAG_CLOSING) > 0) {
            // Close connection if flagged regardless of its last data
            socket_conn_t *next = conn->next;
            _close_conn(conn, hsocket);
            conn = next;
            continue;
        } else if ((conn->flags & SOCKET_CONN_FLAG_LISTENING) > 0) {
            // Check for new connections
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int conn_fd = accept(conn->fd, (struct sockaddr *)&client_addr, &addr_len);
            if (conn_fd < 0) {
                conn = conn->next;
                continue;
            }
            _add_connection(conn_fd, false, hsocket);
            hsocket->config.event_cb(SOCKET_EVENT_CONNECTED, conn->next);
        } else if ((conn->flags & SOCKET_CONN_FLAG_READ_READY) > 0) {
            // Read the connection's payload
            int data_len = read(conn->fd, conn->payload.data, sizeof(conn->payload));
            if (data_len < 0 && errno == EAGAIN) {
                // No data yet...
            } else if (data_len < 0) {
                conn->flags |= SOCKET_CONN_FLAG_CLOSING;
            } else {
                conn->last_activity = time(NULL);
                conn->payload.len = data_len;
                hsocket->config.event_cb(SOCKET_EVENT_DATA_RECEIVED, conn);
            }
        }
        conn = conn->next;
    }

    // Handle connections flagged for closing
    _close_flagged_conns(hsocket);
}

void _check_for_expired_conn(socket_handle_t *hsocket) {
    time_t now = time(NULL);
    for (socket_conn_t *conn = hsocket->connections; conn != NULL; conn = conn->next) {
        if ((conn->flags & SOCKET_CONN_FLAG_LISTENING) > 0) continue;

        double idle = difftime(now, conn->last_activity);
        double total = difftime(now, conn->connected_at);

        // Connection closing should be initiated by the host in order to clear any host specific resources
        if (idle > SOCKET_IDLE_TTL_SECONDS) {
            hsocket->config.event_cb(SOCKET_EVENT_TTL_IDLE_EXPIRED, conn);
        } else if (total > SOCKET_ABSOLUTE_TTL_SECONDS) {
            hsocket->config.event_cb(SOCKET_EVENT_TTL_ABS_EXPIRED, conn);
        }
    }
}

void _add_connection(int fd, bool is_listen_fd, socket_handle_t *hsocket) {
    socket_conn_t *new_conn = malloc(sizeof(socket_conn_t));
    new_conn->connected_at = time(NULL);
    new_conn->last_activity = time(NULL);
    new_conn->fd = fd;
    new_conn->flags = is_listen_fd ? SOCKET_CONN_FLAG_LISTENING : 0;
    new_conn->prev = hsocket->connections_tail;
    new_conn->next = NULL;

    if (hsocket->connections_tail == NULL) {
        // First connection in list
        hsocket->connections = new_conn;
    } else {
        hsocket->connections_tail->next = new_conn;
    }

    hsocket->connections_tail = new_conn;
}

void _close_flagged_conns(socket_handle_t *hsocket) {
    socket_conn_t *conn = hsocket->connections;
    while (conn != NULL) {
        socket_conn_t *next = conn->next;
        if ((conn->flags & SOCKET_CONN_FLAG_CLOSING) > 0) {
            _close_conn(conn, hsocket);
        }
        conn = next;
    }
}

void _close_conn(socket_conn_t *conn, socket_handle_t *hsocket) {
    if (conn->prev != NULL) {
        conn->prev->next = conn->next;
    } else {
        hsocket->connections = conn->next;
    }

    if (conn->next != NULL) {
        conn->next->prev = conn->prev;
    } else {
        hsocket->connections_tail = conn->prev;
    }

    hsocket->config.event_cb(SOCKET_EVENT_DISCONNECTED, NULL);
    close(conn->fd);
    free(conn);
}
