#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#include "http.h"
#include "server.h"
#include "lib/log/log.h"
#include "config.h"

static void route_handler_health(server_client_t *client) {
    char body[] = "Server running normally!";

    server_response_t response = {
        .status = HTTP_OK,
        .headers = NULL,
        .headers_len = 0,
        .body = body,
        .body_len = strlen(body),
        .client = client
    };
    if (server_respond(&response) != 0) {
        log_error("Failed to send HTTP response!");
    }
}

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

    server_route_register("/", route_handler_health);
    server_route_register("/health", route_handler_health);

    server_start();
    return 0;
}