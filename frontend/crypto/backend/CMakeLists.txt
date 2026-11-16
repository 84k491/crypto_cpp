cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(backend STATIC ${PROJECT_SOURCES})

target_include_directories(backend
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(backend PUBLIC
    nlohmann_json::nlohmann_json
    crossguid
    crypto_local
    optimizer
    gateway
    strategy
    trading_primitives
    trading_engine
    network
    util
    ta
    codec
    remote
)
