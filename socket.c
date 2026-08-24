//
// Created by Kok on 8/23/26.
//

#include "socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

static void _poll_socket(socket_handle_t *hsocket);
static void _add_connection(int fd, bool is_listen_fd, socket_handle_t *hsocket);
static void _close_flagged_conns(socket_handle_t *hsocket);

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
    int ret = select(max_fd + 1, &fdset, NULL, NULL, NULL);
    if (ret < 0) {
        perror("socket_error");
        return;
    }

    // Check which connection are ready to be read
    for (socket_conn_t *conn = hsocket->connections; conn != NULL; conn = conn->next) {
        if (FD_ISSET(conn->fd, &fdset)) conn->flags |= SOCKET_CONN_FLAG_READ_READY;
    }

    for (socket_conn_t *conn = hsocket->connections; conn != NULL; conn = conn->next) {
        if ((conn->flags & SOCKET_CONN_FLAG_LISTENING) > 0) {
            // Check for new connections
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int conn_fd = accept(conn->fd, (struct sockaddr *)&client_addr, &addr_len);
            if (conn_fd < 0) continue;
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
                conn->payload.len = data_len;
                hsocket->config.event_cb(SOCKET_EVENT_DATA_RECEIVED, conn);
            }
        }
    }

    // Handle connections flagged for closing
    _close_flagged_conns(hsocket);
}

void _add_connection(int fd, bool is_listen_fd, socket_handle_t *hsocket) {
    socket_conn_t **last_conn = &hsocket->connections;

    socket_conn_t *new_conn = malloc(sizeof(socket_conn_t));
    new_conn->fd = fd;
    new_conn->flags = is_listen_fd ? SOCKET_CONN_FLAG_LISTENING : 0;
    new_conn->next = NULL;

    if (*last_conn == NULL) {
        *last_conn = new_conn;
    } else {
        while ((*last_conn)->next != NULL) last_conn = &(*last_conn)->next;
        (*last_conn)->next = new_conn;
    }
}

void _close_flagged_conns(socket_handle_t *hsocket) {
    socket_conn_t **head = &hsocket->connections;
    while (*head != NULL) {
        socket_conn_t *curr = *head;
        if ((curr->flags & SOCKET_CONN_FLAG_CLOSING) > 0) {
            hsocket->config.event_cb(SOCKET_EVENT_DISCONNECTED, NULL);
            *head = curr->next;
            close(curr->fd);
            free(curr);
        } else {
            head = &(*head)->next;
        }
    }
}
