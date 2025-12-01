find_package(Qt5 COMPONENTS Core Gui Network Test REQUIRED)

add_executable(xnet_body_tests
    xnet_body_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/xnet.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/flowtracer.cpp
)

target_link_libraries(xnet_body_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::Test
    eva_doctest
)

target_include_directories(xnet_body_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(xnet_body_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(xnet_body_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(xnet_body_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME xnet_body_tests COMMAND xnet_body_tests)
set_tests_properties(xnet_body_tests PROPERTIES LABELS unit)

add_executable(xnet_stream_tests
    xnet_stream_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/xnet.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/flowtracer.cpp
)

target_link_libraries(xnet_stream_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::Test
    eva_doctest
)

target_include_directories(xnet_stream_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(xnet_stream_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(xnet_stream_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(xnet_stream_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME xnet_stream_tests COMMAND xnet_stream_tests)
set_tests_properties(xnet_stream_tests PROPERTIES LABELS unit)
