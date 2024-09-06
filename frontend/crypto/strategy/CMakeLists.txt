cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(strategy STATIC ${PROJECT_SOURCES})

target_include_directories(strategy
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(strategy PRIVATE
    nlohmann_json::nlohmann_json
    trading_primitives
    util
    gateway # needed for exit strat. TODO remove
    ta
)
