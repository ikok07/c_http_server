//
// Created by Kok on 8/23/26.
//

#ifndef SOCKETS_TEST_HTTP_H
#define SOCKETS_TEST_HTTP_H
#include <stdlib.h>
#include "config.h"

#define HTTP_REQ_FLAG_HEADERS_COMPLETE              (1UL << 0)      // Indicates that the headers part is handled
#define HTTP_REQ_FLAG_BODY_INCOMPLETE               (1UL << 1)      // Indicates that there is still body data to be received

typedef enum {
    HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_OPTIONS
} http_method_t;

typedef enum {
    HTTP_OK                     = 200,
    HTTP_NO_CONTENT             = 204,
    HTTP_BAD_REQUEST            = 400,
    HTTP_UNAUTHORIZED           = 401,
    HTTP_FORBIDDEN              = 403,
    HTTP_NOT_FOUND              = 404,
    HTTP_METHOD_NOT_ALLOWED     = 405
} http_status_t;

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

typedef struct {
    http_status_t status;
    http_version_t version;
    http_header_t headers[HTTP_MAX_HEADERS];
    int header_count;
    char *body;
    size_t body_len;
} http_response_t;

int http_parse_req(char *data, size_t len, http_request_t *req);
void http_req_free(http_request_t *req);

int http_create_response(http_response_t *resp, char *out, size_t len, size_t *out_len);

#endif //SOCKETS_TEST_HTTP_H
