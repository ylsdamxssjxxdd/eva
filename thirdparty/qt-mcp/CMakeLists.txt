cmake_minimum_required(VERSION 3.16)

project(qtmcp LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.12 REQUIRED COMPONENTS Core Network)

add_library(qtmcp
    src/config.cpp
    src/mcpclient.cpp
    src/sseclient.cpp
    src/streamablehttpclient.cpp
    src/stdioclient.cpp
    include/qmcp/mcpclient.h
    include/qmcp/sseclient.h
    include/qmcp/streamablehttpclient.h
    include/qmcp/stdioclient.h
)

target_include_directories(qtmcp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(qtmcp
    PUBLIC
        Qt5::Core
        Qt5::Network
)

add_executable(qtmcp_example
    examples/example_main.cpp
)

target_link_libraries(qtmcp_example
    PRIVATE
        qtmcp
        Qt5::Core
        Qt5::Network
)
