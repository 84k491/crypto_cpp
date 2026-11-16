cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(remote STATIC ${PROJECT_SOURCES})

target_include_directories(remote
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(remote PUBLIC
    nlohmann_json::nlohmann_json
    crossguid
    codec
    util
)
