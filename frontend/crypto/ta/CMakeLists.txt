cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(ta STATIC ${PROJECT_SOURCES})

target_include_directories(ta
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(ta PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../util
)

target_link_libraries(ta PRIVATE
    nlohmann_json::nlohmann_json
    trading_primitives
)

add_subdirectory(tests)
