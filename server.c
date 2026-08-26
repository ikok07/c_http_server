//
// Created by Kok on 8/24/26.
//

#include "server.h"

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#include "http.h"
#include "log.h"
#include "socket.h"
#include "queue.h"

static queue_t g_client_req_queue = {
    .head = 0, .tail = 0, .count = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .not_empty = PTHREAD_COND_INITIALIZER
};
static socket_handle_t g_hsocket = {0};
static server_clients_t g_active_clients = {0};

static server_routes_t g_registered_routes = {0};

/**
 * @brief Get the HTTP request accumulator associated with a client connection.
 * @param conn Client socket connection.
 * @return http_request_t* Request state pointer, or NULL if not found.
 */
static http_request_t* _get_client_req(socket_conn_t *conn);

/**
 * @brief Register a newly connected client in active client tracking.
 * @param conn Client socket connection.
 * @return int 0 on success, non-zero on failure.
 */
static int _register_client(socket_conn_t *conn);

/**
 * @brief Deregister an active client, free request state, and close connection.
 * @param conn Client socket connection.
 */
static void _deregister_client(socket_conn_t *conn);

/**
 * @brief Worker loop that consumes queued client requests.
 * @param arg Unused thread argument.
 * @return void* Always NULL.
 */
static void *_req_handler(void *arg);

/**
 * @brief Parse incoming request data and dispatch to matching route callback.
 * @param client Active client with newly received payload.
 */
static void _new_http_req_cb(server_client_t *client);

/**
 * @brief Handle socket lifecycle and data events.
 * @param event Socket event type.
 * @param payload Event payload (typically socket_conn_t*).
 */
static void _socket_cb(socket_event_t event, void *payload);

/**
 * @brief Resolve a registered route by URI.
 * @param uri Request URI.
 * @param route Output pointer for matched route.
 * @return int 0 when matched, non-zero when no route matches.
 */
static int _get_server_route(char *uri, server_route_t **route);

/**
 * @brief Send a default status-only HTTP response.
 * @param status HTTP status code to send.
 * @param client Target client connection.
 * @param arg Unused callback argument.
 */
static void _send_default_resp(http_status_t status, server_client_t *client, void *arg);

int server_init(server_config_t *config) {
    memcpy(g_hsocket.config.address, config->address, strlen(config->address) + 1);
    g_hsocket.config.port = config->port;
    g_hsocket.config.max_conn_count = config->max_conn_count;
    g_hsocket.config.event_cb = _socket_cb;

    int ret;
    if ((ret = socket_init(&g_hsocket)) != 0) {
        return ret;
    }

    for (int i = 0; i < REQ_HANDLER_THREAD_POOL_SIZE; i++) {
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, _req_handler, NULL);
        pthread_detach(thread_id);
    }

    return 0;
}

int server_start() {
    return socket_listen(&g_hsocket);
}

int server_respond(server_response_t *response) {
    http_header_t default_headers[2] = {
        {.name = "Content-Length"},
        {.name = "Connection",.value = "close"} // For now we close the connection every time
    };
    size_t default_headers_len = sizeof(default_headers) / sizeof(default_headers[0]);

    char content_len_str[12];
    snprintf(content_len_str, sizeof(content_len_str), "%lu", response->body_len);
    strncpy(default_headers[0].value, content_len_str, sizeof(default_headers[0].value));

    if (response->headers_len + default_headers_len > HTTP_MAX_HEADERS) return 1;

    http_response_t resp = {
        .status = response->status,
        .version = response->client->req.version,
        .header_count = response->headers_len + default_headers_len,
        .body = response->body,
        .body_len = response->body_len
    };
    memcpy(resp.headers, response->headers, response->headers_len * sizeof(http_header_t));
    memcpy(resp.headers + response->headers_len, default_headers, default_headers_len * sizeof(http_header_t));

    char resp_str[1024];
    size_t resp_len;
    if (http_create_response(&resp, resp_str, sizeof(resp_str), &resp_len) != 0) {
        log_error("Failed to create HTTP response!");
        return 1;
    }

    if (socket_write(resp_str, resp_len, response->client->conn) != 0) {
        log_error("Failed to write HTTP response to socket!");
        return 1;
    }

    return 0;
}

int server_route_register(char *uri, route_cb_t cb) {
    if
    (
        g_registered_routes.len >= MAX_ROUTES_COUNT ||
        strlen(uri) > MAX_ROUTE_URI_LEN
    ) return 1;

    g_registered_routes.routes[g_registered_routes.len].cb = cb;
    strncpy(g_registered_routes.routes[g_registered_routes.len].uri, uri, MAX_ROUTE_URI_LEN);
    g_registered_routes.len++;
    return 0;
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

int _register_client(socket_conn_t *conn) {
    if (g_active_clients.clients != NULL) {
        // Check if there is already HTTP request linked to this client connection
        for (int i = 0; i < g_active_clients.len; i++) {
            if (g_active_clients.clients[i].conn->fd == conn->fd) {
                // *out_req = &g_active_clients.clients[i].req;
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

    // *out_req = &g_active_clients.clients[g_active_clients.len - 1].req;
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

void *_req_handler(void *arg) {
    (void)arg;
    while (true) {
        server_client_t *client = queue_pop(&g_client_req_queue);
        _new_http_req_cb(client);
    }
    return NULL;
}

void _new_http_req_cb(server_client_t *client) {
    socket_conn_t *conn = client->conn;
    http_request_t *req = _get_client_req(conn);
    if (req == NULL) {
        log_error("Failed to get client's assigned HTTP request structure!");
        return;
    }

    int ret = http_parse_req(conn->payload.data, conn->payload.len, req);

    if (ret == 0) {
        // Request is complete
        server_route_t *matched_route;
        if (_get_server_route(client->req.uri, &matched_route) == 0) {
            matched_route->cb(client);
        } else {
            _send_default_resp(HTTP_NOT_FOUND, client, NULL);
        }

        _deregister_client(conn);
    } else if (ret == 1) {
        log_error("Invalid HTTP request!\n");
        _deregister_client(conn);
    } else if (ret == 2) {
        // More body chunks are expected...
    }
}

void _socket_cb(socket_event_t event, void *payload) {
    switch (event) {
        case SOCKET_EVENT_LISTENING:
            log_info("Server listening at: %s:%d\n", g_hsocket.config.address, g_hsocket.config.port);
            break;
        case SOCKET_EVENT_CONNECTED: {
            socket_conn_t *conn = payload;
            if (_register_client(conn) != 0) {
                log_error("Failed to register new client!");
                return;
            }

            log_debug("New device connected to server!\n");
            break;
        }
        case SOCKET_EVENT_DISCONNECTED:
            log_debug("Device disconnected from server!\n");
            break;
        case SOCKET_EVENT_TTL_IDLE_EXPIRED:
        case SOCKET_EVENT_TTL_ABS_EXPIRED: {
            socket_conn_t *conn = payload;
            log_info("Connection expired!\n");
            _deregister_client(conn);
            break;
        }
        case SOCKET_EVENT_DATA_RECEIVED: {
            socket_conn_t *conn = payload;
            log_debug("Data received! Length: %lu\n", conn->payload.len);
            for (int i = 0; i < g_active_clients.len; i++) {
                if (g_active_clients.clients[i].conn->fd == conn->fd) {
                    queue_push(&g_active_clients.clients[i], &g_client_req_queue);
                    return;
                }
            }

            // Hopefully never reaches here
            log_error("New HTTP request received, however there is no active client linked to this socket!");
            break;
        }
    }
}

int _get_server_route(char *uri, server_route_t **route) {
    // TODO: Implement more complex URI matching algorithm
    for (int i = 0; i < g_registered_routes.len; i++) {
        if (strcmp(g_registered_routes.routes[i].uri, uri) == 0) {
            *route = g_registered_routes.routes;
            return 0;
        }
    }
    return 1;
}

void _send_default_resp(http_status_t status, server_client_t *client, void *arg) {
    server_response_t response = {
        .status = status,
        .headers = NULL, .body = NULL,
        .client = client
    };
    if (server_respond(&response) != 0) {
        log_error("Failed to send default response with status %d!", status);
    };
}

