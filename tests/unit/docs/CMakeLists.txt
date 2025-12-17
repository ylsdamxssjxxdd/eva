find_package(Qt5 COMPONENTS Core REQUIRED)

add_executable(doc2md_tests
    doc2md_tests.cpp
)

target_link_libraries(doc2md_tests PRIVATE
    Qt5::Core
    eva_doctest
    doc2md_lib
    miniz
)
target_include_directories(doc2md_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(doc2md_tests PRIVATE cxx_std_17)
target_compile_definitions(doc2md_tests PRIVATE EVA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(doc2md_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(doc2md_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()
