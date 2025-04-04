cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ..
    ../interfaces
)

##############################
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

##############################
add_executable(candle_builder_test
    CandleBuilderTest.cpp
)

target_link_libraries(candle_builder_test
    ${GTEST_BOTH_LIBRARIES}
    crossguid2
    trading_primitives
    nlohmann_json
)

set(UNIT_TEST candle_builder_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})

##############################
add_executable(ordinary_least_squares_test
    OrdinaryLeastSquaresTest.cpp
    ../OrdinaryLeastSquares.cpp
)

target_link_libraries(ordinary_least_squares_test
    ${GTEST_BOTH_LIBRARIES}
)

set(UNIT_TEST ordinary_least_squares_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})

