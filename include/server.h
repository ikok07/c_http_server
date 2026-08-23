//
// Created by Kok on 8/23/26.
//

#ifndef SOCKETS_TEST_SOCKETS_H
#define SOCKETS_TEST_SOCKETS_H
#include <stddef.h>
#include <stdint.h>

/* ----- STATUS FLAGS ------ */
#define SERVER_CONN_FLAG_LISTENING                  (1UL << 0)
#define SERVER_CONN_FLAG_READ_READY                 (1UL << 1)
#define SERVER_CONN_FLAG_CLOSING                    (1UL << 2)

typedef enum {
    SERVER_EVENT_STARTED,
    SERVER_EVENT_CONNECTED,
    SERVER_EVENT_DISCONNECTED,
    SERVER_EVENT_DATA_RECEIVED
} server_event_t;

typedef void(*server_event_cb_t)(server_event_t event, void *payload);

typedef struct {
    char data[512];
    size_t len;
} server_conn_payload_t;

typedef struct server_conn {
    struct server_conn *next;
    int fd;
    uint8_t flags;
    server_conn_payload_t payload;
} server_conn_t;

typedef struct {
    int port;
    char address[15];
    int max_conn_count;
    server_event_cb_t event_cb;
} server_config_t;

typedef struct {
    server_conn_t *connections;
    server_config_t config;
} server_handle_t;

/**
 * @brief Initializes server socket
 * @param hserver Server handle
 * @return 0 - OK; 1 - ERROR
 */
int server_init(server_handle_t *hserver);

/**
 * @brief Starts listening on the configured address and port
 * @param hserver Server handle
 * @return 1 - ERROR; Normally it shouldn't return
 */
int server_listen(server_handle_t *hserver);

/**
 * @brief Closes connection to a client
 * @param conn Connection structure
 * @param hserver Server handle
 */
void server_close_connection(server_conn_t *conn, server_handle_t *hserver);

#endif //SOCKETS_TEST_SOCKETS_H
