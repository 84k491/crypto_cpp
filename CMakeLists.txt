cmake_minimum_required(VERSION 3.10)

project(crypto_platform)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(crypto)
add_subdirectory(json)
add_subdirectory(frontend)
set(JSON_BuildTests OFF CACHE INTERNAL "")
