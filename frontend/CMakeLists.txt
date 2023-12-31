cmake_minimum_required(VERSION 3.5)

project(frontend VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Charts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        DragableChart.cpp
        ../crypto/src/ByBitGateway.cpp
        ../crypto/src/ohlc.cpp
        ../crypto/src/DoubleSmaStrategy.cpp
        ../crypto/src/ScopeExit.cpp
        ../crypto/src/Signal.cpp
        ../crypto/src/Timerange.cpp
        ../crypto/src/StrategyInstance.cpp
        ../crypto/src/Position.cpp
        ../crypto/src/Optimizer.cpp
        ../crypto/ta/src/SimpleMovingAverage.cpp
        ../crypto/src/BollingerBandsStrategy.cpp
        ../crypto/src/StrategyFactory.cpp
        ../crypto/ta/src/BollingerBands.cpp
        ../crypto/ta/src/StandardDeviation.cpp
        ../crypto/src/WorkerThread.cpp
)

qt_add_executable(frontend
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
)
target_compile_options(frontend PRIVATE "-g" "-O0")

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${PROJECT_SOURCE_DIR}/../crypto/interfaces
    ${PROJECT_SOURCE_DIR}/../crypto/include
    ${PROJECT_SOURCE_DIR}/../crypto/ta/include
)

target_link_libraries(frontend PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(frontend PRIVATE
    Qt6::Charts
    curl
    nlohmann_json::nlohmann_json
    crossguid2
)

include(GNUInstallDirs)
install(TARGETS frontend
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
 if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(frontend)
endif()
