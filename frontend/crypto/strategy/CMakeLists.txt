cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

find_package(pybind11 REQUIRED)

add_library(strategy STATIC ${PROJECT_SOURCES})

target_include_directories(strategy
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(strategy PRIVATE
    nlohmann_json::nlohmann_json
    trading_primitives
    trading_engine
    util
    gateway # needed for exit strat. TODO remove
    ta
    pybind11::embed
)

add_subdirectory(tests)
