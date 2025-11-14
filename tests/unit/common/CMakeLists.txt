add_library(eva_doctest INTERFACE)
target_include_directories(eva_doctest INTERFACE ${CMAKE_SOURCE_DIR}/thirdparty/doctest)

add_library(eva_test_support INTERFACE)
target_include_directories(eva_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
