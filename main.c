#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#include "http.h"
#include "server.h"
#include "lib/log/log.h"

#define SERVER_ADDR         "127.0.0.1"
#define SERVER_PORT         9000
#define SERVER_MAX_CONNS    128

int main(void) {
    log_set_level(1);                   // DEBUG level
    log_set_quiet(false);

    server_config_t server_config = {
        .address = SERVER_ADDR,
        .port = SERVER_PORT,
        .max_conn_count = SERVER_MAX_CONNS,
    };
    if (server_init(&server_config) != 0) {
        if (errno == EADDRINUSE) {
            log_error("Port %d already in use", SERVER_PORT);
        } else if (errno == EINVAL) {
            log_error("Invalid server address! %s", SERVER_ADDR);
        } else {
            log_error("server_init: %s", strerror(errno));
        }
        return 1;
    };

    server_start();
    return 0;
}