find_package(Qt5 COMPONENTS Core Gui REQUIRED)

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

add_executable(tool_registry_tests
    tool_registry_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/service/tools/tool_registry.cpp
)
target_link_libraries(tool_registry_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    eva_doctest
)
target_include_directories(tool_registry_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)
target_compile_features(tool_registry_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(tool_registry_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(tool_registry_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME tool_registry_tests COMMAND tool_registry_tests)
set_tests_properties(tool_registry_tests PROPERTIES LABELS unit)
