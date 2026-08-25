//
// Created by Kok on 8/24/26.
//

#ifndef C_HTTP_SERVER_SERVER_H
#define C_HTTP_SERVER_SERVER_H

#include "socket.h"
#include "http.h"

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

int server_init(server_config_t *config);
int server_start();

#endif //C_HTTP_SERVER_SERVER_H
