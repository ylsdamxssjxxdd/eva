find_package(Qt5 COMPONENTS Core REQUIRED)

add_executable(prompt_builder_tests
    prompt_builder_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt_builder.cpp
)
target_link_libraries(prompt_builder_tests PRIVATE
    Qt5::Core
    eva_doctest
)
target_include_directories(prompt_builder_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(prompt_builder_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(prompt_builder_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(prompt_builder_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME prompt_builder_tests COMMAND prompt_builder_tests)
set_tests_properties(prompt_builder_tests PROPERTIES LABELS unit)
