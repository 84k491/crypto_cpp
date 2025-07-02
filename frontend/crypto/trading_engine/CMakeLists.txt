cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(trading_engine STATIC ${PROJECT_SOURCES})

target_include_directories(trading_engine
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(trading_engine PUBLIC
    nlohmann_json::nlohmann_json
    trading_primitives
    gateway
    util
)

add_subdirectory(tests)
