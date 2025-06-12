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
    util
    nlohmann_json
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
    trading_engine
    util
    gateway
    nlohmann_json
    strategy
)
set(UNIT_TEST strategy_instance_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})

####################################################################################################
add_executable(backtest_trading_gateway_test
    BacktestTradingGatewayTest.cpp
)
target_link_libraries(backtest_trading_gateway_test
    ${GTEST_BOTH_LIBRARIES}
    trading_primitives
    trading_engine
    util
    gateway
    nlohmann_json
)
set(UNIT_TEST backtest_trading_gateway_test) # TODO remove it?
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})

####################################################################################################
add_executable(bybit_trading_gateway_LIVE_test
    BybitTradingGatewayLiveTest.cpp
)
target_link_libraries(bybit_trading_gateway_LIVE_test
    ${GTEST_BOTH_LIBRARIES}
    trading_primitives
    trading_engine
    util
    gateway
    network
    nlohmann_json
)
set(UNIT_TEST bybit_trading_gateway_LIVE_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})
