# ThirdParty.cmake - bring in and configure external deps

# Prevent double-include
include_guard(GLOBAL)

# Always keep these as in-tree: small, stable libraries we link directly
add_subdirectory(thirdparty/QHotkey)

add_library(miniz STATIC
    ${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.c
    ${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz_zip.c
    ${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz_tinfl.c
    ${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz_tdef.c)
target_include_directories(miniz PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/miniz)

add_subdirectory(thirdparty/doc2md/thirdparty/libxls)

add_library(tinyxml2 STATIC ${CMAKE_SOURCE_DIR}/thirdparty/doc2md/thirdparty/tinyxml2/tinyxml2.cpp)
target_include_directories(tinyxml2 PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/doc2md/thirdparty/tinyxml2)

add_library(doc2md_lib STATIC
    ${CMAKE_SOURCE_DIR}/thirdparty/doc2md/src/doc2md/document_converter.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/doc2md/src/doc2md/detail_parsers.cpp)
target_include_directories(doc2md_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/thirdparty/doc2md/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/doc2md/src
        ${CMAKE_SOURCE_DIR}/thirdparty/doc2md/thirdparty/libxls/include
        ${CMAKE_SOURCE_DIR}/thirdparty/doc2md/thirdparty/tinyxml2
        ${CMAKE_SOURCE_DIR}/thirdparty/miniz)
target_compile_features(doc2md_lib PRIVATE cxx_std_11)
target_link_libraries(doc2md_lib PUBLIC miniz tinyxml2 libxls)

# libsamplerate/libsndfile no longer required; whisper-cli handles resampling

if (WIN32)
    set(sfx_NAME ".exe")
elseif(UNIX)
    set(sfx_NAME "")
endif()

###
# ------------------------------ Prebuilt Backends Copy ------------------------------
# Instead of building llama.cpp / whisper.cpp / stable-diffusion.cpp here,
# copy prebuilt artifacts from <source>/EVA_BACKEND to <build>/bin/EVA_BACKEND.
set(BACKEND_SOURCE_DIR ${CMAKE_SOURCE_DIR}/EVA_BACKEND)
set(BACKEND_DEST_DIR   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/EVA_BACKEND)

if (EXISTS "${BACKEND_SOURCE_DIR}")
    add_custom_target(backends ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BACKEND_DEST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${BACKEND_SOURCE_DIR}" "${BACKEND_DEST_DIR}"
        COMMENT "Copying prebuilt backends from ${BACKEND_SOURCE_DIR} to ${BACKEND_DEST_DIR}"
    )
else()
    add_custom_target(backends
        COMMENT "No 'EVA_BACKEND' folder in source; skipping EVA_BACKEND copy. Provide it manually.")
    message(WARNING "No EVA_BACKEND/ folder at project root. EVA will run without local backends. Place your prebuilt backends under 'EVA_BACKEND/'.")
endif()


# ------------------------------ MCP ------------------------------
add_subdirectory(thirdparty/qt-mcp)
