cmake_minimum_required(VERSION 3.10)

project(crypto_platform)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2b")

add_subdirectory(crypto)
add_subdirectory(json)
add_subdirectory(frontend)
set(JSON_BuildTests OFF CACHE INTERNAL "")
