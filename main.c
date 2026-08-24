#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>

#include "http.h"
#include "server.h"

#define SERVER_ADDR         "127.0.0.1"
#define SERVER_PORT         9000
#define SERVER_MAX_CONNS    128

int main(void) {
    server_config_t server_config = {
        .address = SERVER_ADDR,
        .port = SERVER_PORT,
        .max_conn_count = SERVER_MAX_CONNS,
    };
    if (server_init(&server_config) != 0) {
        if (errno == EADDRINUSE) {
            fprintf(stderr, "Port %d already in use", SERVER_PORT);
        } else if (errno == EINVAL) {
            fprintf(stderr, "Invalid server address! %s", SERVER_ADDR);
        } else {
            perror("server_init");
        }
    };

    server_start();
    return 0;
}