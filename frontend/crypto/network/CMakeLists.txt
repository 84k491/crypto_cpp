cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(network STATIC ${PROJECT_SOURCES})

target_include_directories(network
    PUBLIC
    ${PROJECT_SOURCE_DIR}/../thirdparty/websocketpp
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(network PRIVATE
    nlohmann_json::nlohmann_json
    trading_primitives
    crypto
    curl
    ssl
    util
)
