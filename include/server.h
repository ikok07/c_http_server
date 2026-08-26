//
// Created by Kok on 8/24/26.
//

#ifndef C_HTTP_SERVER_SERVER_H
#define C_HTTP_SERVER_SERVER_H

#include "socket.h"
#include "http.h"
#include "config.h"

typedef struct {
    int port;
    char address[15];
    int max_conn_count;
} server_config_t;

typedef struct {
    socket_conn_t *conn;
    http_request_t req;
} server_client_t;

typedef struct {
    server_client_t *clients;
    size_t len;
} server_clients_t;

typedef void(*route_cb_t)(server_client_t *client);

typedef struct {
    char uri[MAX_ROUTE_URI_LEN];
    route_cb_t cb;
} server_route_t;

typedef struct {
    server_route_t routes[MAX_ROUTES_COUNT];
    size_t len;
} server_routes_t;

typedef struct {
    http_status_t status;
    char *body;
    size_t body_len;
    http_header_t *headers;
    size_t headers_len;
    server_client_t *client;
} server_response_t;

/**
 * @brief Initialize server socket configuration and request worker threads.
 * @param config Server configuration values (address, port, max connections).
 * @return int 0 on success, non-zero on failure.
 */
int server_init(server_config_t *config);

/**
 * @brief Start the blocking socket listen loop.
 * @return int 0 on success, non-zero on failure.
 */
int server_start();

/**
 * @brief Build and send an HTTP response to a client connection.
 * @param response Response payload, headers, status, and target client.
 * @return int 0 on success, non-zero on serialization or socket write failure.
 */
int server_respond(server_response_t *response);

/**
 * @brief Register a callback for an exact URI route match.
 * @param uri Route URI string.
 * @param cb Callback invoked when the route is matched.
 * @return int 0 on success, non-zero when route registration fails.
 */
int server_route_register(char *uri, route_cb_t cb);

#endif //C_HTTP_SERVER_SERVER_H
