cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ..
)
####################################################################################################
add_executable(position_manager_test
    PositionManagerTest.cpp
    ../PositionManager.cpp
)
target_link_libraries(position_manager_test
    ${GTEST_BOTH_LIBRARIES}
    trading_primitives
)
set(UNIT_TEST position_manager_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})
####################################################################################################
add_executable(strategy_instance_test
    StrategyInstanceTest.cpp
    ../StrategyInstance.cpp
    ../PositionManager.cpp
)
target_link_libraries(strategy_instance_test
    ${GTEST_BOTH_LIBRARIES}
    trading_primitives
    util
    gateway
    nlohmann_json
    strategy
)
set(UNIT_TEST strategy_instance_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})
