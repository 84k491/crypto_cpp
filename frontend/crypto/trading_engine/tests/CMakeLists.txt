cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ..
)

####################################################################################################
add_executable(order_manager_test
    OrderManagerTest.cpp
    ../OrderManager.cpp
)
target_link_libraries(order_manager_test
    ${GTEST_BOTH_LIBRARIES}
    trading_primitives
    util
    nlohmann_json
    trading_engine
)
set(UNIT_TEST order_manager_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})
