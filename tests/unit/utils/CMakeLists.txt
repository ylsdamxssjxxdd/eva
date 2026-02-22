find_package(Qt5 COMPONENTS Core Gui Network REQUIRED)

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

add_executable(perf_metrics_tests
    perf_metrics_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/perf_metrics.cpp
)
target_link_libraries(perf_metrics_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    eva_doctest
)
target_include_directories(perf_metrics_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)
target_compile_features(perf_metrics_tests PRIVATE cxx_std_17)

add_executable(backend_lifecycle_tests
    backend_lifecycle_tests.cpp
)
target_link_libraries(backend_lifecycle_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    eva_doctest
)
target_include_directories(backend_lifecycle_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)
target_compile_features(backend_lifecycle_tests PRIVATE cxx_std_17)

add_executable(settings_change_analyzer_tests
    settings_change_analyzer_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/settings_change_analyzer.cpp
)
target_link_libraries(settings_change_analyzer_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    eva_doctest
)
target_include_directories(settings_change_analyzer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)
target_compile_features(settings_change_analyzer_tests PRIVATE cxx_std_17)

add_executable(eva_error_tests
    eva_error_tests.cpp
)
target_link_libraries(eva_error_tests PRIVATE
    Qt5::Core
    eva_doctest
)
target_include_directories(eva_error_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(eva_error_tests PRIVATE cxx_std_17)

add_executable(net_retry_policy_tests
    net_retry_policy_tests.cpp
)
target_link_libraries(net_retry_policy_tests PRIVATE
    Qt5::Core
    Qt5::Network
    eva_doctest
)
target_include_directories(net_retry_policy_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(net_retry_policy_tests PRIVATE cxx_std_17)

add_executable(recovery_guidance_tests
    recovery_guidance_tests.cpp
)
target_link_libraries(recovery_guidance_tests PRIVATE
    Qt5::Core
    eva_doctest
)
target_include_directories(recovery_guidance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(recovery_guidance_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(pathutil_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(processrunner_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(zip_extractor_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(perf_metrics_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(backend_lifecycle_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(settings_change_analyzer_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(eva_error_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(net_retry_policy_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(recovery_guidance_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(pathutil_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(processrunner_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(zip_extractor_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(perf_metrics_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(backend_lifecycle_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(settings_change_analyzer_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(eva_error_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(net_retry_policy_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(recovery_guidance_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME pathutil_tests COMMAND pathutil_tests)
add_test(NAME processrunner_tests COMMAND processrunner_tests)
add_test(NAME zip_extractor_tests COMMAND zip_extractor_tests)
add_test(NAME perf_metrics_tests COMMAND perf_metrics_tests)
add_test(NAME backend_lifecycle_tests COMMAND backend_lifecycle_tests)
add_test(NAME settings_change_analyzer_tests COMMAND settings_change_analyzer_tests)
add_test(NAME eva_error_tests COMMAND eva_error_tests)
add_test(NAME net_retry_policy_tests COMMAND net_retry_policy_tests)
add_test(NAME recovery_guidance_tests COMMAND recovery_guidance_tests)
set_tests_properties(pathutil_tests processrunner_tests zip_extractor_tests perf_metrics_tests backend_lifecycle_tests settings_change_analyzer_tests eva_error_tests net_retry_policy_tests recovery_guidance_tests PROPERTIES LABELS unit)
