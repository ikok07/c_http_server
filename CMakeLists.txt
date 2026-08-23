cmake_minimum_required(VERSION 4.3)
project(c_http_server C)

set(CMAKE_C_STANDARD 23)

set (INCLUDES include)
file (GLOB SOURCES
    "./*.c"
)

include_directories(${INCLUDES})
add_executable(sockets_test ${SOURCES})
