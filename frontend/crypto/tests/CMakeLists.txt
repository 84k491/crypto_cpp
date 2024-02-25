cmake_minimum_required(VERSION 3.10)

enable_testing()
find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    ..
    ../interfaces
)

add_executable(object_publisher_test
    ObjectPublisherTest.cpp
)

target_link_libraries(object_publisher_test
    ${GTEST_BOTH_LIBRARIES}
    crossguid2
)

add_test(NAME object_publisher_test COMMAND object_publisher_test)
