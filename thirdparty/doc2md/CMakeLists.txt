cmake_minimum_required(VERSION 3.16)

project(doc2md LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "Forcing static linking for all toolchains")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
add_compile_definitions(_STATIC_CPPLIB)

option(DOC2MD_BUILD_TESTS "Build doc2md unit tests" OFF)
include(CTest)

add_library(miniz STATIC
    thirdparty/miniz/miniz.c
    thirdparty/miniz/miniz_zip.c
    thirdparty/miniz/miniz_tinfl.c
    thirdparty/miniz/miniz_tdef.c)
target_include_directories(miniz PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/miniz)

add_subdirectory(thirdparty/libxls)

add_library(tinyxml2 STATIC thirdparty/tinyxml2/tinyxml2.cpp)
target_include_directories(tinyxml2 PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/tinyxml2)

add_subdirectory(src)
add_subdirectory(examples)
if(DOC2MD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
