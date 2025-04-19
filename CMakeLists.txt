cmake_minimum_required(VERSION 3.10)

project(crypto_platform)
enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CustomBuildType)
    set(CustomBuildType "Debug")
endif()

if (CustomBuildType)
    message(STATUS "Custom build type: ${CustomBuildType}")
endif()

if(CustomBuildType STREQUAL "Debug")
    # add_definitions( -DDEBUG=1 )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -ggdb \
        -g3 \
        -O0 \
        ")
elseif(CustomBuildType STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -O3 \
        ")
elseif(CustomBuildType STREQUAL "Asan")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -ggdb \
        -g3 \
        -O0 \
        -fsanitize=address \
        ")
elseif(CustomBuildType STREQUAL "Tsan")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -ggdb \
        -g3 \
        -O0 \
        -fsanitize=thread \
        ")
else()
    message(WARNING "Unknown build type: ${CustomBuildType}")
endif()

add_subdirectory(thirdparty/fmt)
add_subdirectory(thirdparty/json)
add_subdirectory(thirdparty/crossguid)
add_subdirectory(thirdparty/QCustomPlot-library)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -std=c++20 \
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
    ")

add_subdirectory(frontend)
set(JSON_BuildTests OFF CACHE INTERNAL "")
