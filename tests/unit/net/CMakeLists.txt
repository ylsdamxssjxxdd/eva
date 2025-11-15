find_package(Qt5 COMPONENTS Core Gui Network REQUIRED)

add_executable(xnet_body_tests
    xnet_body_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/xnet.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt_builder.cpp
)

target_link_libraries(xnet_body_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Network
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
