cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(codec STATIC ${PROJECT_SOURCES})

target_include_directories(codec
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../backend
)

target_link_libraries(codec PUBLIC
    nlohmann_json::nlohmann_json
    crossguid
    trading_primitives
    strategy
    util
)
