//
// Created by Kok on 8/26/26.
//

#ifndef C_HTTP_SERVER_CONFIG_H
#define C_HTTP_SERVER_CONFIG_H

/* ------ HTTP Server Config ------ */
#define SERVER_ADDR                                 "127.0.0.1"
#define SERVER_PORT                                 9000
#define SERVER_MAX_CONNS                            128
#define REQ_HANDLER_THREAD_POOL_SIZE                4
#define MAX_ROUTE_URI_LEN                           512
#define MAX_ROUTES_COUNT                            32

/* ------ HTTP Parser Config ------ */
#define HTTP_MAX_HEADER_LEN                         8096                // The header part of the HTTP request cannot exceed this threshold
#define HTTP_MAX_BODY_LEN                           1024000             // The maximum length of the HTTP request's body

#define HTTP_MAX_HEADERS                            32
#define HTTP_MAX_HEADER_NAME                        64
#define HTTP_MAX_HEADER_VALUE                       256

/* ------ Sockets Config ------ */
#define SOCKET_WAIT_TIMEOUT_SECONDS                 (2)
#define SOCKET_ABSOLUTE_TTL_SECONDS                 (300)            // The maximum allowed time for each socket connection to be closed
#define SOCKET_IDLE_TTL_SECONDS                     (30)             // The maximum allowed time for each socket connection to receive some data
#define SOCKET_WRITE_TIMEOUT_MS                     (3000)

#endif //C_HTTP_SERVER_CONFIG_H
