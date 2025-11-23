add_executable(doc2md_cli doc2md_cli/main.cpp)
target_link_libraries(doc2md_cli PRIVATE doc2md_lib)
target_include_directories(doc2md_cli PRIVATE ${CMAKE_SOURCE_DIR}/include)
