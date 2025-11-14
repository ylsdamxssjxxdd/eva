find_package(Qt5 COMPONENTS Core REQUIRED)

add_executable(pathutil_tests
    pathutil_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/pathutil.cpp
)
target_link_libraries(pathutil_tests PRIVATE
    Qt5::Core
    eva_doctest
)
target_include_directories(pathutil_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(pathutil_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(pathutil_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(pathutil_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME pathutil_tests COMMAND pathutil_tests)
set_tests_properties(pathutil_tests PROPERTIES LABELS unit)
