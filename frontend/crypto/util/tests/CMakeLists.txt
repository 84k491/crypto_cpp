cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ..
    ../interfaces
)

add_executable(event_loop_test
    EventLoopTest.cpp
)

target_link_libraries(event_loop_test
    ${GTEST_BOTH_LIBRARIES}
    crossguid2
    trading_primitives
    nlohmann_json
)

set(UNIT_TEST event_loop_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})

# add_custom_command(
#      TARGET ${UNIT_TEST}
#      COMMENT "Run test: ${UNIT_TEST}"
#      POST_BUILD 
#      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#      COMMAND ${UNIT_TEST}
# )
