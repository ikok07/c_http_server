//
// Created by Kok on 8/23/26.
//

#ifndef SOCKETS_TEST_HTTP_H
#define SOCKETS_TEST_HTTP_H
#include <stdlib.h>

#define HTTP_MAX_HEADER_LEN                         8096                // The header part of the HTTP request cannot exceed this threshold
#define HTTP_MAX_BODY_LEN                           1024000             // The maximum length of the HTTP request's body

#define HTTP_MAX_HEADERS                            32
#define HTTP_MAX_HEADER_NAME                        64
#define HTTP_MAX_HEADER_VALUE                       256

#define HTTP_REQ_FLAG_HEADERS_COMPLETE              (1UL << 0)      // Indicates that the headers part is handled
#define HTTP_REQ_FLAG_BODY_INCOMPLETE               (1UL << 1)      // Indicates that there is still body data to be received

typedef enum {
    HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_OPTIONS
} http_method_t;

typedef enum {
    HTTP_VERSION_09,
    HTTP_VERSION_10,
    HTTP_VERSION_11,
    HTTP_VERSION_20,
    HTTP_VERSION_30
} http_version_t;

typedef struct {
    char name[HTTP_MAX_HEADER_NAME];
    char value[HTTP_MAX_HEADER_VALUE];
} http_header_t;

typedef struct {
    uint8_t flags;
    char *temp;                 // Used to store temp data. It is set to NULL when its not used
    size_t temp_len;
    http_method_t method;
    char *uri;
    http_version_t version;
    http_header_t headers[HTTP_MAX_HEADERS];
    int header_count;
    char *body;
    size_t body_len;
} http_request_t;

// TODO: Add free request method

int parse_http_req(char *data, size_t len, http_request_t *req);
void http_req_free(http_request_t *req);

#endif //SOCKETS_TEST_HTTP_H
