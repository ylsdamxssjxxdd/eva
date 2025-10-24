cmake_minimum_required(VERSION 3.12)

set(XTOOL_TEST_SOURCES
    xtool_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/xtool.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/tinyexpr/tinyexpr.c
)

find_package(Qt5 COMPONENTS Test REQUIRED)

add_executable(xtool_tests ${XTOOL_TEST_SOURCES})

target_include_directories(xtool_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/thirdparty/cpp-mcp/include
    ${CMAKE_SOURCE_DIR}/thirdparty/cpp-mcp/common
)

target_link_libraries(xtool_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
    Qt5::Concurrent
    Qt5::Network
    mcp
)

if (UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(xtool_tests PRIVATE X11::X11 X11::Xtst)
endif()

target_compile_features(xtool_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(xtool_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(xtool_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME xtool_tests COMMAND xtool_tests)
