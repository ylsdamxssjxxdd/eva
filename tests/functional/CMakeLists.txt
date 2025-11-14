set(EVA_FUNCTIONAL_SCENARIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scenarios)

find_package(Qt5 COMPONENTS Test REQUIRED)

add_executable(eva_functional_tests functional_smoke_tests.cpp)
target_link_libraries(eva_functional_tests PRIVATE
    Qt5::Core
    Qt5::Test
)
target_compile_features(eva_functional_tests PRIVATE cxx_std_17)
target_compile_definitions(eva_functional_tests PRIVATE
    EVA_FUNCTIONAL_SCENARIO_DIR="${EVA_FUNCTIONAL_SCENARIO_DIR}"
)

add_test(NAME eva_functional_tests COMMAND eva_functional_tests)
set_tests_properties(eva_functional_tests PROPERTIES LABELS functional)
