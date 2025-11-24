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

add_executable(processrunner_tests
    processrunner_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/processrunner.cpp
)
target_link_libraries(processrunner_tests PRIVATE
    Qt5::Core
    eva_doctest
)
target_include_directories(processrunner_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(processrunner_tests PRIVATE cxx_std_17)

add_executable(zip_extractor_tests
    zip_extractor_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/zip_extractor.cpp
)
target_link_libraries(zip_extractor_tests PRIVATE
    Qt5::Core
    eva_doctest
    miniz
)
target_include_directories(zip_extractor_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(zip_extractor_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(pathutil_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(processrunner_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(zip_extractor_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(pathutil_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(processrunner_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(zip_extractor_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME pathutil_tests COMMAND pathutil_tests)
add_test(NAME processrunner_tests COMMAND processrunner_tests)
add_test(NAME zip_extractor_tests COMMAND zip_extractor_tests)
set_tests_properties(pathutil_tests processrunner_tests zip_extractor_tests PROPERTIES LABELS unit)
