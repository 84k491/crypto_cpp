cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_subdirectory(ta)
add_subdirectory(util)
add_subdirectory(strategy)
add_subdirectory(gateway)
add_subdirectory(trading_primitives)
add_subdirectory(network)

add_library(crypto_local STATIC ${PROJECT_SOURCES})

target_include_directories(crypto_local
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(crypto_local PRIVATE
    curl
    nlohmann_json::nlohmann_json
    crossguid2
    ssl
    crypto
    ta
    strategy
    gateway
    trading_primitives
    network
    util
)
