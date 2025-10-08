# ThirdParty.cmake - bring in and configure external deps

# Prevent double-include
include_guard(GLOBAL)

# Always keep these as in-tree: small, stable libraries we link directly
add_subdirectory(thirdparty/QHotkey)
add_subdirectory(thirdparty/libsamplerate)
add_subdirectory(thirdparty/libsndfile)
list(APPEND extra_LIBS samplerate sndfile)

if (WIN32)
    set(sfx_NAME ".exe")
elseif(UNIX)
    set(sfx_NAME "")
endif()

###
# ------------------------------ Prebuilt Backends Copy ------------------------------
# Instead of building llama.cpp / whisper.cpp / stable-diffusion.cpp here,
# copy prebuilt artifacts from <source>/backend to <build>/bin/backend.
set(BACKEND_SOURCE_DIR ${CMAKE_SOURCE_DIR}/backend)
set(BACKEND_DEST_DIR   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/backend)

if (EXISTS "${BACKEND_SOURCE_DIR}")
    add_custom_target(backends ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BACKEND_DEST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${BACKEND_SOURCE_DIR}" "${BACKEND_DEST_DIR}"
        COMMENT "Copying prebuilt backends from ${BACKEND_SOURCE_DIR} to ${BACKEND_DEST_DIR}"
    )
else()
    add_custom_target(backends
        COMMENT "No 'backend' folder in source; skipping backend copy. Provide it manually.")
    message(WARNING "No backend/ folder at project root. EVA will run without local backends. Place your prebuilt backends under 'backend/'.")
endif()


# ------------------------------ MCP ------------------------------
add_subdirectory(thirdparty/cpp-mcp)
include_directories(thirdparty/cpp-mcp/include)
include_directories(thirdparty/cpp-mcp/common)




