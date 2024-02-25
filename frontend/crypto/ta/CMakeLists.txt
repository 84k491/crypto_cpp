cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(ta STATIC ${PROJECT_SOURCES})

target_include_directories(ta
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
