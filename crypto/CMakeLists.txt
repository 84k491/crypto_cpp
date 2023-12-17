cmake_minimum_required(VERSION 3.10)

project(crypto)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ByBitGateway.cpp
    src/ohlc.cpp
    src/DoubleSmaStrategy.cpp
    src/ScopeExit.cpp
    src/Signal.cpp
    src/Timerange.cpp
    src/StrategyInstance.cpp
    src/Position.cpp
    ta/src/SimpleMovingAverage.cpp
    src/BollingerBandsStrategy.cpp
)
target_compile_options(crypto PRIVATE "-g" "-Og")

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/ta/include
    ${PROJECT_SOURCE_DIR}/../RESTinCurl/include/restincurl
)

target_link_libraries(${PROJECT_NAME}
    curl
    nlohmann_json::nlohmann_json
)
