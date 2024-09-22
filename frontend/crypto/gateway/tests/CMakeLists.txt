cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ..
)
################################################
add_executable(bybit_tr_messages_test
    BybitTradingMessagesTest.cpp
)
target_link_libraries(bybit_tr_messages_test
    ${GTEST_BOTH_LIBRARIES}
    gateway
    trading_primitives
    util
    nlohmann_json
)
set(UNIT_TEST bybit_tr_messages_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})
