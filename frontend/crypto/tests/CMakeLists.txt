cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ..
)

add_executable(position_manager_test
    PositionManagerTest.cpp
    ../PositionManager.cpp
)

target_link_libraries(position_manager_test
    ${GTEST_BOTH_LIBRARIES}
    trading_primitives
    # crossguid2
)

set(UNIT_TEST position_manager_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})
# add_custom_command(
#      TARGET ${UNIT_TEST}
#      COMMENT "Run test: ${UNIT_TEST}"
#      POST_BUILD 
#      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#      COMMAND ${UNIT_TEST}
# )
