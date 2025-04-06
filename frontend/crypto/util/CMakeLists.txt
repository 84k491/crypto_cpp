cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_subdirectory(tests)

add_library(util STATIC ${PROJECT_SOURCES})

target_include_directories(util
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(util PUBLIC
    trading_primitives
    nlohmann_json::nlohmann_json
    crossguid
)
