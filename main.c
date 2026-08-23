#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>

#include "http.h"
#include "server.h"

#define SERVER_ADDR         "127.0.0.1"
#define SERVER_PORT         9000
#define SERVER_MAX_CONNS    128

void server_cb(server_event_t event, void *payload);

server_handle_t hserver = {
    .config = {
        .address = SERVER_ADDR,
        .port = SERVER_PORT,
        .max_conn_count = SERVER_MAX_CONNS,
        .event_cb = server_cb
    }
};

int main(void) {
    if (server_init(&hserver) != 0) {
        if (errno == EADDRINUSE) {
            fprintf(stderr, "Port %d already in use", hserver.config.port);
        } else if (errno == EINVAL) {
            fprintf(stderr, "Invalid server address! %s", SERVER_ADDR);
        } else {
            perror("server_init");
        }
        return 1;
    }

    server_listen(&hserver);
    return 0;
}

void server_cb(server_event_t event, void *payload) {
    switch (event) {
        case SERVER_EVENT_STARTED:
            printf("Server started at: %s:%d\n", SERVER_ADDR, SERVER_PORT);
            break;
        case SERVER_EVENT_CONNECTED:
            printf("New device connected to server!\n");
            break;
        case SERVER_EVENT_DISCONNECTED:
            printf("Device disconnected from server!\n");
            break;
        case SERVER_EVENT_DATA_RECEIVED: {
            // TODO: Data might not be a complete HTTP request with / without chunk.
            server_conn_t *conn = payload;
            printf("Data received! Length: %lu\n", conn->payload.len);

            static bool prev_req = false;       // Indicate whether the data is a chunk from previous request's body
            static http_request_t req = {0};

            int ret;
            if (prev_req) {
                ret = parse_http_req_body_chunk(conn->payload.data, conn->payload.len, &req);
            } else {
                ret = parse_http_req(conn->payload.data, conn->payload.len, &req);
            }

            if (ret == 0) {
                // Request is complete
                prev_req = false;
                http_req_free(&req);
                server_close_connection(conn, &hserver);
            } else if (ret == 1) {
                if (errno == EINVAL) {
                    fprintf(stderr, "Invalid HTTP request!");
                } else {
                    perror("http_parse");
                }
            } else if (ret == 2) {
                // More body chunks are expected...
                prev_req = true;
            }

            break;
        }
    }
}