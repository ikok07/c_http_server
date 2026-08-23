//
// Created by Kok on 8/23/26.
//

#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

static void _poll_socket(server_handle_t *hserver);
static void _add_connection(int fd, bool is_listen_fd, server_handle_t *hserver);
static void _close_flagged_conns(server_handle_t *hserver);

int server_init(server_handle_t *hserver) {
    struct in_addr in_addr;
    if (inet_aton(hserver->config.address, &in_addr) == 0) {
        errno = EINVAL;
        return 1;
    }

    struct sockaddr_in sock_addr = {.sin_family = AF_INET, .sin_port = htons(hserver->config.port), .sin_addr = {.s_addr = in_addr.s_addr}};

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
    _add_connection(listen_fd, true, hserver);

    return 0;
}

int server_listen(server_handle_t *hserver) {
    int ret = listen(hserver->connections->fd, hserver->config.max_conn_count);
    if (ret < 0) return 1;

    hserver->config.event_cb(SERVER_EVENT_STARTED, NULL);
    while (true) _poll_socket(hserver);

    return 0;
}

void server_close_connection(server_conn_t *conn, server_handle_t *hserver) {
    conn->flags |= SERVER_CONN_FLAG_CLOSING;
    _close_flagged_conns(hserver);
}

void _poll_socket(server_handle_t *hserver) {
    fd_set fdset;
    FD_ZERO(&fdset);
    int max_fd = 0;

    // Add all connections to the set
    for (server_conn_t *conn = hserver->connections; conn != NULL; conn = conn->next) {
        FD_SET(conn->fd, &fdset);
        if (conn->fd > max_fd) max_fd = conn->fd;
    }

    // Wait for some network event
    int ret = select(max_fd + 1, &fdset, NULL, NULL, NULL);
    if (ret < 0) {
        perror("server_error");
        return;
    }

    // Check which connection are ready to be read
    for (server_conn_t *conn = hserver->connections; conn != NULL; conn = conn->next) {
        if (FD_ISSET(conn->fd, &fdset)) conn->flags |= SERVER_CONN_FLAG_READ_READY;
    }

    for (server_conn_t *conn = hserver->connections; conn != NULL; conn = conn->next) {
        if ((conn->flags & SERVER_CONN_FLAG_LISTENING) > 0) {
            // Check for new connections
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int conn_fd = accept(conn->fd, (struct sockaddr *)&client_addr, &addr_len);
            if (conn_fd < 0) continue;
            _add_connection(conn_fd, false, hserver);
            hserver->config.event_cb(SERVER_EVENT_CONNECTED, conn);
        } else if ((conn->flags & SERVER_CONN_FLAG_READ_READY) > 0) {
            // Read the connection's payload
            int data_len = read(conn->fd, conn->payload.data, sizeof(conn->payload));
            if (data_len < 0 && errno == EAGAIN) {
                // No data yet...
            } else if (data_len < 0) {
                conn->flags = SERVER_CONN_FLAG_CLOSING;
            } else {
                conn->payload.len = data_len;
                hserver->config.event_cb(SERVER_EVENT_DATA_RECEIVED, conn);
            }
        }
    }

    // Handle connections flagged for closing
    _close_flagged_conns(hserver);
}

void _add_connection(int fd, bool is_listen_fd, server_handle_t *hserver) {
    server_conn_t **last_conn = &hserver->connections;

    server_conn_t *new_conn = malloc(sizeof(server_conn_t));
    new_conn->fd = fd;
    new_conn->flags = is_listen_fd ? SERVER_CONN_FLAG_LISTENING : 0;
    new_conn->next = NULL;

    if (*last_conn == NULL) {
        *last_conn = new_conn;
    } else {
        while ((*last_conn)->next != NULL) last_conn = &(*last_conn)->next;
        (*last_conn)->next = new_conn;
    }
}

void _close_flagged_conns(server_handle_t *hserver) {
    server_conn_t **head = &hserver->connections;
    while (*head != NULL) {
        server_conn_t *curr = *head;
        if ((curr->flags & SERVER_CONN_FLAG_CLOSING) > 0) {
            hserver->config.event_cb(SERVER_EVENT_DISCONNECTED, NULL);
            *head = curr->next;
            close(curr->fd);
            free(curr);
        } else {
            head = &(*head)->next;
        }
    }
}
