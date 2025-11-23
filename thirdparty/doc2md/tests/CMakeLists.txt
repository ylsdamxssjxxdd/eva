add_executable(doc2md_tests test_runner.cpp)
target_link_libraries(doc2md_tests PRIVATE doc2md_lib)
target_include_directories(doc2md_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(doc2md_tests PRIVATE DOC2MD_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests")

add_test(NAME doc2md_parse_all_tests COMMAND doc2md_tests)
