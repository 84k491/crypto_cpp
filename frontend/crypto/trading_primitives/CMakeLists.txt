cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(trading_primitives STATIC ${PROJECT_SOURCES})

target_include_directories(trading_primitives
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(trading_primitives PRIVATE
    nlohmann_json::nlohmann_json
    util
)
