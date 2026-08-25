cmake_minimum_required(VERSION 4.3)
project(c_http_server C)

set(CMAKE_C_STANDARD 23)

set (INCLUDES include lib/log)
file (GLOB SOURCES
    "./*.c"
    "./lib/log/*.c"
)

include_directories(${INCLUDES})
add_executable(c_http_server ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE LOG_USE_COLOR)
