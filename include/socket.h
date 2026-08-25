//
// Created by Kok on 8/23/26.
//

#ifndef SOCKETS_TEST_SOCKETS_H
#define SOCKETS_TEST_SOCKETS_H
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* ------ CONFIG ------ */
#define SOCKET_WAIT_TIMEOUT_SECONDS                 (2)
#define SOCKET_ABSOLUTE_TTL_SECONDS                 (300)            // The maximum allowed time for each socket connection to be closed
#define SOCKET_IDLE_TTL_SECONDS                     (30)             // The maximum allowed time for each socket connection to receive some data

/* ----- STATUS FLAGS ------ */
#define SOCKET_CONN_FLAG_LISTENING                  (1UL << 0)
#define SOCKET_CONN_FLAG_READ_READY                 (1UL << 1)
#define SOCKET_CONN_FLAG_CLOSING                    (1UL << 2)

typedef enum {
    SOCKET_EVENT_LISTENING,
    SOCKET_EVENT_CONNECTED,
    SOCKET_EVENT_DISCONNECTED,
    SOCKET_EVENT_TTL_IDLE_EXPIRED,
    SOCKET_EVENT_TTL_ABS_EXPIRED,
    SOCKET_EVENT_DATA_RECEIVED
} socket_event_t;

typedef void(*socket_event_cb_t)(socket_event_t event, void *payload);

typedef struct {
    char data[512];
    size_t len;
} socket_conn_payload_t;

typedef struct socket_conn {
    struct socket_conn *prev;
    struct socket_conn *next;
    int fd;
    uint8_t flags;
    socket_conn_payload_t payload;
    time_t connected_at;
    time_t last_activity;
} socket_conn_t;

typedef struct {
    int port;
    char address[15];
    int max_conn_count;
    socket_event_cb_t event_cb;
} socket_config_t;

typedef struct {
    socket_conn_t *connections;
    socket_conn_t *connections_tail;
    socket_config_t config;
} socket_handle_t;

/**
 * @brief Initializes server socket
 * @param hsocket Socket handle
 * @return 0 - OK; 1 - ERROR
 */
int socket_init(socket_handle_t *hsocket);

/**
 * @brief Starts listening on the configured address and port
 * @param hsocket Socket handle
 * @return 1 - ERROR; Normally it shouldn't return
 */
int socket_listen(socket_handle_t *hsocket);

/**
 * @brief Writes data to the specified socket connection
 * @param data Data to send
 * @param len Length of the data
 * @param conn Socket connection
 * @return 0 - OK; 1 - ERROR
 */
int socket_write(char *data, size_t len, socket_conn_t *conn);

/**
 * @brief Closes connection to a client
 * @param conn Connection structure
 */
void socket_close_connection(socket_conn_t *conn);

#endif //SOCKETS_TEST_SOCKETS_H
