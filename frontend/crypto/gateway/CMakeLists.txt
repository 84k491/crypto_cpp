cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(gateway STATIC ${PROJECT_SOURCES})

target_include_directories(gateway
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gateway PRIVATE
    nlohmann_json::nlohmann_json
    util
    network
    trading_primitives
)
