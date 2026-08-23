//
// Created by Kok on 8/23/26.
//

#ifndef SOCKETS_TEST_HTTP_H
#define SOCKETS_TEST_HTTP_H
#include <stdlib.h>

#define MAX_HEADERS             32
#define MAX_HEADER_NAME         64
#define MAX_HEADER_VALUE        256

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
    char name[MAX_HEADER_NAME];
    char value[MAX_HEADER_VALUE];
} http_header_t;

typedef struct {
    http_method_t method;
    char *uri;
    http_version_t version;
    http_header_t headers[MAX_HEADERS];
    int header_count;
    char *body;
} http_request_t;

// TODO: Add free request method

int parse_http_req(char *data, size_t len, http_request_t *req);
int parse_http_req_body_chunk(char *data, size_t len, http_request_t *req);
void http_req_free(http_request_t *req);

#endif //SOCKETS_TEST_HTTP_H
