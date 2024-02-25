cmake_minimum_required(VERSION 3.5)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_subdirectory(ta)

add_library(crypto_local STATIC ${PROJECT_SOURCES})

# target_include_directories(${PROJECT_NAME}
target_include_directories(crypto_local
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/ta
    ${PROJECT_SOURCE_DIR}/../json/include
)

target_link_libraries(crypto_local PRIVATE
    Qt6::Charts
    curl
    nlohmann_json::nlohmann_json
    crossguid2
    ssl
    crypto
    ta
)
