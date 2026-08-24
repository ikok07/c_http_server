//
// Created by Kok on 8/24/26.
//

#include "server.h"

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#include "http.h"
#include "socket.h"

socket_handle_t g_hsocket = {0};
server_clients_t g_active_clients = {0};

http_request_t* _get_client_req(socket_conn_t *conn);
int _register_client(socket_conn_t *conn, http_request_t **out_req);
void _deregister_client(socket_conn_t *conn);

void socket_cb(socket_event_t event, void *payload);

int server_init(server_config_t *config) {
    memcpy(g_hsocket.config.address, config->address, strlen(config->address) + 1);
    g_hsocket.config.port = config->port;
    g_hsocket.config.max_conn_count = config->max_conn_count;
    g_hsocket.config.event_cb = socket_cb;

    int ret;
    if ((ret = socket_init(&g_hsocket)) != 0) {
        return ret;
    }
}

int server_start() {
    return socket_listen(&g_hsocket);
}

http_request_t* _get_client_req(socket_conn_t *conn) {
    if (g_active_clients.clients == NULL) return NULL;
    // Check if there is already HTTP request linked to this client connection
    for (int i = 0; i < g_active_clients.len; i++) {
        if (g_active_clients.clients[i].conn->fd == conn->fd) {
            return &g_active_clients.clients[i].req;
        }
    }
    return NULL;
}

int _register_client(socket_conn_t *conn, http_request_t **out_req) {
    if (g_active_clients.clients != NULL) {
        // Check if there is already HTTP request linked to this client connection
        for (int i = 0; i < g_active_clients.len; i++) {
            if (g_active_clients.clients[i].conn->fd == conn->fd) {
                *out_req = &g_active_clients.clients[i].req;
                return 0;
            }
        }
    }

    // Create new http request structure for this client connection
    server_client_t *new_buf = realloc(g_active_clients.clients, (g_active_clients.len + 1) * sizeof(server_client_t));
    if (new_buf == NULL) return 1;
    g_active_clients.clients = new_buf;
    g_active_clients.len++;

    server_client_t new_client = {
        .conn = conn,
        .req = {0}
    };
    memcpy(g_active_clients.clients + g_active_clients.len - 1, &new_client, sizeof(new_client));

    *out_req = &g_active_clients.clients[g_active_clients.len - 1].req;
    return 0;
}

void _deregister_client(socket_conn_t *conn) {
    if (g_active_clients.clients == NULL) return;
    // Check if there is already HTTP request linked to this client connection
    int target_idx = -1;
    for (int i = 0; i < g_active_clients.len; i++) {
        if (g_active_clients.clients[i].conn->fd == conn->fd) {
            target_idx = i;
            break;
        }
    }
    if (target_idx < 0) return;

    http_req_free(&g_active_clients.clients[target_idx].req);
    g_active_clients.clients[target_idx].conn = NULL;

    socket_close_connection(conn);
    g_active_clients.len--;
}

void socket_cb(socket_event_t event, void *payload) {
    switch (event) {
        case SOCKET_EVENT_LISTENING:
            printf("Server started at: %s:%d\n", g_hsocket.config.address, g_hsocket.config.port);
            break;
        case SOCKET_EVENT_CONNECTED:
            printf("New device connected to server!\n");
            break;
        case SOCKET_EVENT_DISCONNECTED:
            printf("Device disconnected from server!\n");
            break;
        case SOCKET_EVENT_DATA_RECEIVED: {
            socket_conn_t *conn = payload;
            printf("Data received! Length: %lu\n", conn->payload.len);

            http_request_t *req = _get_client_req(conn);
            if (req == NULL) {
                if (_register_client(conn, &req) != 0) {
                    fprintf(stderr, "Failed to register new client!");
                    return;
                }
            }

            int ret = parse_http_req(conn->payload.data, conn->payload.len, req);

            if (ret == 0) {
                // Request is complete
                _deregister_client(conn);
            } else if (ret == 1) {
                if (errno == EINVAL) {
                    fprintf(stderr, "Invalid HTTP request!");
                } else {
                    perror("http_parse");
                }
            } else if (ret == 2) {
                // More body chunks are expected...
            }

            break;
        }
    }
}
