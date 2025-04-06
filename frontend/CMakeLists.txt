cmake_minimum_required(VERSION 3.5)

project(frontend VERSION 0.1 LANGUAGES CXX)

add_subdirectory(crypto)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Charts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        MultiSeriesChart.h
        MultiSeriesChart.cpp
        StrategyParametersWidget.h
        StrategyParametersWidget.cpp
        OptimizerParamsWidget.h
        OptimizerParamsWidget.cpp
)

qt_add_executable(frontend
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        chart_window.h chart_window.cpp chart_window.ui
        PositionResultView.h PositionResultView.cpp PositionResultView.ui
)

target_include_directories(frontend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(frontend PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    qcustomplot
    nlohmann_json::nlohmann_json
    crossguid
    crypto_local
    ta
    util
    strategy
    gateway
    trading_primitives
    network
)

# Compile needed definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE QCUSTOMPLOT_USE_LIBRARY)

include(GNUInstallDirs)
install(TARGETS frontend
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(frontend)
endif()
