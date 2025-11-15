find_package(Qt5 COMPONENTS Core Gui Network Test REQUIRED)

add_executable(mcp_logic_tests
    mcp_logic_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/xmcp_internal.cpp
)

target_link_libraries(mcp_logic_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    eva_doctest
)

target_include_directories(mcp_logic_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty/qt-mcp/include
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)

target_compile_features(mcp_logic_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(mcp_logic_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(mcp_logic_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME mcp_logic_tests COMMAND mcp_logic_tests)
set_tests_properties(mcp_logic_tests PROPERTIES LABELS unit)

add_executable(xmcp_tests
    xmcp_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/xmcp.cpp
    ${CMAKE_SOURCE_DIR}/src/xmcp_internal.cpp
)

target_link_libraries(xmcp_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::Test
    qtmcp
)

target_include_directories(xmcp_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty/qt-mcp/include
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
    ${CMAKE_SOURCE_DIR}/thirdparty/doctest
)

target_compile_features(xmcp_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(xmcp_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(xmcp_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME xmcp_tests COMMAND xmcp_tests)
set_tests_properties(xmcp_tests PROPERTIES LABELS unit)
