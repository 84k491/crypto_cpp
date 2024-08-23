cmake_minimum_required(VERSION 3.10)

project(crypto_platform)
enable_testing()

set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(thirdparty/json)
add_subdirectory(thirdparty/QCustomPlot-library)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -std=c++23 \
    -Wall \
    -Wextra \
    -Wshadow \
    -Wswitch-bool \
    -Wswitch \
    -Wreorder-ctor \
    -Wunused-parameter \
    -Wunused-private-field \
    -Wstrict-overflow=5 \
    -Wunreachable-code \
    -Wuninitialized \
    -Wpedantic \
    -Winit-self \
    -Wpointer-arith \
    -Wredundant-decls \
    -Wthread-safety \
    -Wconsumed \
    -Warray-bounds-pointer-arithmetic \
    -Wsign-compare \
    -Wimplicit-fallthrough \
    -ggdb \
    -g3 \
    -O0 \
    ")
    # -fsanitize=address \

add_subdirectory(frontend)
set(JSON_BuildTests OFF CACHE INTERNAL "")
