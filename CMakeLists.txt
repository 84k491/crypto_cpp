cmake_minimum_required(VERSION 3.10)

project(crypto_platform)

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
    ")

add_subdirectory(json)
add_subdirectory(frontend)
set(JSON_BuildTests OFF CACHE INTERNAL "")
