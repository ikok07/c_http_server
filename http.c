//
// Created by Kok on 8/23/26.
//

#include "http.h"

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

static int _append_temp_data(char *data, size_t len, http_request_t *req);

static int _get_http_method(char *str, http_method_t *out);
static int _get_http_version(char *str, http_version_t *out);
static int _get_http_header_value(char *name, char **out, http_request_t *req);

static int _has_http_header(char *name, http_request_t *req);

static int _parse_http_req_header_line(char *start, size_t len, int line_idx, http_request_t *req);
static int _extract_fixed_size_body(char *start, size_t len, http_request_t *req);
static int _extract_chunked_body(char *start, size_t len, http_request_t *req);

int parse_http_req(char *data, size_t len, http_request_t *req) {
    if (req == NULL) {
        errno = EINVAL;
        return 1;
    }

    if ((req->flags & HTTP_REQ_FLAG_HEADERS_COMPLETE) > 0) {
        // Only request body chunks are expected from now on...
        if (_has_http_header("Content-Length", req)) {
            int ret = _extract_fixed_size_body(data, len, req);
            if (req->body_len > HTTP_MAX_BODY_LEN) return 1;
            if (ret != 0) return ret;
        } else if (_has_http_header("Transfer-Encoding", req)) {
            int ret = _extract_chunked_body(data, len, req);
            if (req->body_len > HTTP_MAX_BODY_LEN) return 1;
            if (ret != 0) return ret;
        }
        return 0;
    }

    int offset = 0;

    // Append the current data to temp buffer
    _append_temp_data(data, len, req);

    char *headers_end = memmem(req->temp, req->temp_len, "\r\n\r\n", 4);
    if (headers_end == NULL) {
        // Request is still incomplete
        if (req->temp_len > HTTP_MAX_HEADER_LEN) return 1;
        return 2;
    }

    // Terminate temp buffer
    char terminator = '\0';
    _append_temp_data(&terminator, 1, req);
    req->flags |= HTTP_REQ_FLAG_HEADERS_COMPLETE;

    // Extract the headers
    int line_idx = 0;
    char *startline = req->temp + offset;
    while (headers_end - startline >= 0) {
        char *endline = memmem(req->temp + offset, req->temp_len - offset, "\r\n", 2);
        if (endline == NULL) return 1;

        printf("%.*s\n", endline - startline, startline);
        if (req->header_count > HTTP_MAX_HEADERS) {
            return 1;
        }

        if (_parse_http_req_header_line(startline, endline - startline, line_idx, req) != 0) {
            return 1;
        }

        // Account for the 2 ending bytes (\r\n)
        offset += (endline + 2) - startline;
        startline = req->temp + offset;
        line_idx++;
    }

    // Extract the body
    char *body_start = headers_end + 4;     // Account for "\r\n\r\n"
    size_t left = (req->temp + req->temp_len - 1) - body_start;

    // Free the temp memory
    free(req->temp);
    req->temp_len = 0;

    if (req->method != HTTP_GET) {
        if (_has_http_header("Content-Length", req)) {
            int ret = _extract_fixed_size_body(body_start, left, req);
            if (req->body_len > HTTP_MAX_BODY_LEN) return 1;
            if (ret != 0) return ret;
        } else if (_has_http_header("Transfer-Encoding", req)) {
            int ret = _extract_chunked_body(body_start, left, req);
            if (req->body_len > HTTP_MAX_BODY_LEN) return 1;
            if (ret != 0) return ret;
        }
    }

    return 0;
}

void http_req_free(http_request_t *req) {
    free(req->body);
    free(req->uri);
    memset(req, 0, sizeof(*req));
}

int _append_temp_data(char *data, size_t len, http_request_t *req) {
    char *new = realloc(req->temp, req->temp_len + len);
    if (new == NULL) return 1;
    req->temp = new;

    memcpy(req->temp + req->temp_len, data, len);
    req->temp_len += len;
}

int _get_http_method(char *str, http_method_t *out) {
    if (memcmp(str, "GET", 3) == 0) *out = HTTP_GET;
    else if (memcmp(str, "POST", 4) == 0) *out = HTTP_POST;
    else if (memcmp(str, "PUT", 3) == 0) *out = HTTP_PUT;
    else if (memcmp(str, "PATCH", 5) == 0) *out = HTTP_PATCH;
    else if (memcmp(str, "OPTIONS", 7) == 0) *out = HTTP_OPTIONS;
    else return 1;

    return 0;
}

int _get_http_version(char *str, http_version_t *out) {
    if (memcmp(str, "HTTP/0.9", 8) == 0) *out = HTTP_VERSION_09;
    else if (memcmp(str, "HTTP/1.0", 8) == 0) *out = HTTP_VERSION_10;
    else if (memcmp(str, "HTTP/1.1", 8) == 0) *out = HTTP_VERSION_11;
    else if (memcmp(str, "HTTP/2", 6) == 0) *out = HTTP_VERSION_20;
    else if (memcmp(str, "HTTP/3", 6) == 0) *out = HTTP_VERSION_30;
    else return 1;

    return 0;
}

int _get_http_header_value(char *name, char **out, http_request_t *req) {
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(name, req->headers[i].name) == 0) {
            *out = req->headers[i].value;
            return 0;
        }
    }
    return 1;
}

int _has_http_header(char *name, http_request_t *req) {
    char *value;
    return _get_http_header_value(name, &value, req) == 0;
}

int _parse_http_req_header_line(char *start, size_t len, int line_idx, http_request_t *req) {
    int offset = 0;

    if (line_idx == 0) {
        // Parse the first line of the request
        int word_idx = 0;
        char *wordstart = start + offset;
        while (offset < len) {
            char *wordend = memmem(wordstart, len - offset, " ", 1);
            if (wordend == NULL) wordend = start + len;

            if (word_idx == 0) {
                // Extract HTTP method
                http_method_t method;
                if (_get_http_method(wordstart, &method) != 0) return 1;
                req->method = method;
            } else if (word_idx == 1) {
                // Extract URI
                char *uri = calloc(1, wordend - wordstart + 1);    // Account for \0 at the end
                memcpy(uri, wordstart, wordend - wordstart);
                uri[wordend - wordstart] = '\0';
                req->uri = uri;
            } else if (word_idx == 2) {
                // Extract HTTP version
                http_version_t version;
                if (_get_http_version(wordstart, &version) != 0) return 1;
                req->version = version;
            }
            word_idx++;

            offset += wordend - wordstart + 1;
            wordstart = start + offset;
        }
    } else {
        // Parse headers
        char *header_sep = memmem(start, len, ":", 1);
        if (header_sep == NULL) return 1;
        int name_end = header_sep - start;

        offset += name_end + 2; // Account for the two symbols after the name (: )

        memcpy(req->headers[req->header_count].name, start, name_end);
        req->headers[req->header_count].name[name_end] = '\0';

        memcpy(req->headers[req->header_count].value, start + offset, len - offset);
        req->headers[req->header_count].value[len - offset] = '\0';

        req->header_count++;
    }

    return 0;
}

int _extract_fixed_size_body(char *start, size_t len, http_request_t *req) {
    size_t body_size = 0;
    char *content_length;
    if (_get_http_header_value("Content-Length", &content_length, req) == 0) {
        char *endptr;
        body_size = strtol(content_length, &endptr, 10);
        if (endptr == content_length) {
            // Invalid "Content-Length" value
            errno = EINVAL;
            return 1;
        }
    } else {
        // Missing "Content-Length" header
        errno = EINVAL;
        return 1;
    }

    if ((req->flags & HTTP_REQ_FLAG_BODY_INCOMPLETE) == 0) {
        // First chunk of the body data
        req->body = calloc(1, body_size);
        memcpy(req->body, start, len);
    } else {
        // Append the new chunk to the body
        memcpy(req->body + req->body_len, start, len);
    }

    req->body_len += len;

    if (req->body_len < body_size) {
        req->flags |= HTTP_REQ_FLAG_BODY_INCOMPLETE;
    } else {
        req->flags &=~ HTTP_REQ_FLAG_BODY_INCOMPLETE;
    }

    if ((req->flags & HTTP_REQ_FLAG_BODY_INCOMPLETE) > 0) return 2;

    return 0;
}

int _extract_chunked_body(char *start, size_t len, http_request_t *req) {
    char *endptr;

    // If length is 0, we shall expect more body chunks
    if (len == 0) return 2;

    size_t chunk_size = strtol(start, &endptr, 16);
    if (endptr == start) {
        // Invalid chunk size
        errno = EINVAL;
        return 1;
    }

    // Indicate last chunk
    if (chunk_size == 0) return 0;

    char *chunk_start = memmem(start, len, "\r\n", 2) + 2;
    if (chunk_start == NULL) return 1;

    if (req->body == NULL) {
        req->body = calloc(1, chunk_size);
        memcpy(req->body, chunk_start, chunk_size);
    } else {
        size_t curr_body_len = req->body_len;
        size_t new_body_len = curr_body_len + chunk_size + 1;
        char *new_body = realloc(req->body, new_body_len);
        if (new_body == NULL) return 1;
        req->body = new_body;

        memcpy(req->body + curr_body_len, chunk_start, chunk_size);
        req->body[curr_body_len + chunk_size] = '\0';
        req->body_len += chunk_size;
    }

    return 2;
}
