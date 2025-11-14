find_package(Qt5 COMPONENTS Core Gui Sql REQUIRED)

add_executable(vectordb_tests
    vectordb_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/vectordb.cpp
)

target_link_libraries(vectordb_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Sql
    eva_doctest
)
target_include_directories(vectordb_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)
target_compile_features(vectordb_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(vectordb_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(vectordb_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME vectordb_tests COMMAND vectordb_tests)
set_tests_properties(vectordb_tests PROPERTIES LABELS unit)
