cmake_minimum_required(VERSION 3.10)

project(crypto_platform)
enable_testing()

set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -std=c++23 \
    -Wall \
    -Wextra \
    -Wshadow \
    -Wswitch-default \
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
    -ggdb \
    -g3 \
    -Og \
    ")

add_subdirectory(thirdparty/json)
add_subdirectory(frontend)
set(JSON_BuildTests OFF CACHE INTERNAL "")
