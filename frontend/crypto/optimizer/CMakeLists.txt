cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(optimizer STATIC ${PROJECT_SOURCES})

target_include_directories(optimizer
    PUBLIC
    .. # TODO remove, this is for strategy instance
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(optimizer PRIVATE
    util
    gateway
    strategy
    network
)
