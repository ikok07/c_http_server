# c_http_server

A small HTTP server written in C for educational purposes, focused on learning BSD sockets, event-driven I/O, and low-level request parsing.

This project is intentionally simple and explicit: the goal is to show the mechanics behind a basic server rather than hide details behind frameworks.

## Why I built this

I built this project to better understand:
- BSD socket lifecycle (`socket`, `bind`, `listen`, `accept`, `read`, `send`, `close`)
- Multiplexing with `select()`
- Request parsing for fragmented TCP payloads
- Thread-safe producer/consumer flow between network I/O and request handling
- How to structure a small C server with clear module boundaries

## Current capabilities

- TCP server on configurable address/port (default `127.0.0.1:9000`)
- Event-driven socket loop with connection tracking
- HTTP request parsing for:
  - Request line and headers
  - `Content-Length` bodies
  - `Transfer-Encoding: chunked` bodies
  - Fragmented incoming data across multiple TCP reads
- Route registration with callback handlers
- Built-in example routes:
  - `GET /`
  - `GET /health`
- Thread pool for request processing (`REQ_HANDLER_THREAD_POOL_SIZE`, default `4`)
- Connection lifecycle events (connected, disconnected, data received, TTL expired)
- Defensive limits for headers/body sizes via config constants

## Architecture overview

The codebase is split into focused modules:

- `socket.c` / `include/socket.h`
  - Socket initialization, listen loop, `select()` polling, non-blocking listen fd
  - Connection linked-list management
  - Read/write handling and connection close signaling
- `http.c` / `include/http.h`
  - HTTP request parser and response serializer
  - Support for status/version labels and body extraction logic
- `server.c` / `include/server.h`
  - High-level server API
  - Active client registry
  - Route lookup and dispatch
  - Worker threads consuming queued client requests
- `queue.c` / `include/queue.h`
  - Mutex/condition-variable protected bounded queue
- `main.c`
  - App entry point, configuration, and sample route registration
- `lib/log/`
  - Lightweight logging utility

## Configuration

Most tunables are in `include/config.h`, including:
- Server address/port and max connection count
- Thread pool size
- Max route count and URI length
- Max header/body limits
- Socket wait/write timeout
- Idle and absolute connection TTL

## Build and run

### Prerequisites

- A C toolchain (e.g., `clang` on macOS)
- CMake (version declared in `CMakeLists.txt`)

### Build

```bash
cmake -S . -B build
cmake --build build
```

### Run

```bash
./build/c_http_server
```

You should see logs indicating the server is listening on `127.0.0.1:9000` (by default).

## Quick checks

### Health endpoint

```bash
curl -v http://127.0.0.1:9000/health
```

Expected body:

```text
Server running normally!
```

### Included test scripts

The repository includes manual stress/behavior scripts:
- `test.sh`: fragmented headers + fixed-size body
- `test2.sh`: fragmented headers + chunked transfer encoding
- `test3.sh`: oversized header test
- `test4.sh`: large body upload test (~1 MB)
- `test5.sh`: simple curl smoke test

Example:

```bash
sh test5.sh
```

## Known limitations (intentional for now)

- No keep-alive/persistent connections (responses currently include `Connection: close`)
- URI matching is exact-string only (no params/wildcards/regex)
- No TLS/HTTPS
- Basic HTTP feature set (not a full production HTTP implementation)
- Limited hardening/performance work compared to production servers

## Ideas for next iterations

- HTTP/1.1 keep-alive and connection reuse
- More expressive route matching (`/users/:id`, wildcard, prefix)
- Better request/response validation and error reporting
- Cleaner active-client lifecycle management
- Benchmarking and profiling under concurrent load
- Unit tests for parser edge cases

## Educational value

This project is a compact reference for understanding how a server works below framework level. It demonstrates system calls, state management, concurrency primitives, and protocol parsing in straightforward C.

---