set(XTOOL_TEST_SOURCES
    xtool_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/xtool.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/docker_sandbox.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/tinyexpr/tinyexpr.c
)

find_package(Qt5 COMPONENTS Test REQUIRED)

add_library(processrunner_test STATIC ${CMAKE_SOURCE_DIR}/src/utils/processrunner.cpp)
target_link_libraries(processrunner_test PRIVATE Qt5::Core)
target_compile_features(processrunner_test PRIVATE cxx_std_17)

add_executable(xtool_tests ${XTOOL_TEST_SOURCES})

target_include_directories(xtool_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)

target_link_libraries(xtool_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Test
    Qt5::Concurrent
    Qt5::Network
    eva_test_support
    qtmcp
    processrunner_test
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
set_tests_properties(xtool_tests PROPERTIES LABELS unit)

add_executable(xtool_harness_tests xtool_harness_tests.cpp)
target_link_libraries(xtool_harness_tests PRIVATE
    eva_doctest
    eva_test_support
)
target_include_directories(xtool_harness_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)
target_compile_features(xtool_harness_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(xtool_harness_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(xtool_harness_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME xtool_harness_tests COMMAND xtool_harness_tests)
set_tests_properties(xtool_harness_tests PROPERTIES LABELS unit)
