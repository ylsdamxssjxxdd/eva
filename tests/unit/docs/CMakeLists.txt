find_package(Qt5 COMPONENTS Core REQUIRED)

add_executable(docparser_tests
    docparser_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/docparser.cpp
)

target_link_libraries(docparser_tests PRIVATE
    Qt5::Core
    eva_doctest
)
target_include_directories(docparser_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(docparser_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(docparser_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(docparser_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME docparser_tests COMMAND docparser_tests)
set_tests_properties(docparser_tests PROPERTIES LABELS unit)
