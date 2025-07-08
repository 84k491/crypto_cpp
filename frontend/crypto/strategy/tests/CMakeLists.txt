cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ..
)

####################################################################################################
add_executable(grid_levels_test
    GridLevelsTest.cpp
    ../GridStrategy.cpp
)
target_link_libraries(grid_levels_test
    ${GTEST_BOTH_LIBRARIES}
    trading_primitives
    util
    nlohmann_json
    strategy
    trading_engine
    ta
)
set(UNIT_TEST grid_levels_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})
