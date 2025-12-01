cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ..
)

####################################################################################################
add_executable(time_weighted_average_test
    TimeWeightedMovingAverageTest.cpp
    ../TimeWeightedMovingAverage.cpp
)
target_link_libraries(time_weighted_average_test
    ${GTEST_BOTH_LIBRARIES}
    util
    nlohmann_json
)
set(UNIT_TEST time_weighted_average_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})

####################################################################################################
add_executable(standard_deviation_test
    StandardDeviationTest.cpp
    ../StandardDeviation.cpp
)
target_link_libraries(standard_deviation_test
    ${GTEST_BOTH_LIBRARIES}
    util
    nlohmann_json
)
set(UNIT_TEST standard_deviation_test)
add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TEST})
