# ThirdParty.cmake - bring in and configure external deps

# Prevent double-include
include_guard(GLOBAL)

# Always keep these as in-tree: small, stable libraries we link directly
add_subdirectory(thirdparty/QHotkey)
# libsamplerate/libsndfile no longer required; whisper-cli handles resampling

if (WIN32)
    set(sfx_NAME ".exe")
elseif(UNIX)
    set(sfx_NAME "")
endif()

set(MCP_SSL ON CACHE BOOL "Enable SSL support for MCP clients" FORCE)

if (WIN32 AND NOT DEFINED OPENSSL_ROOT_DIR)
    set(_EVA_DEFAULT_OPENSSL_ROOT "C:/Program Files/OpenSSL")
    if (EXISTS "${_EVA_DEFAULT_OPENSSL_ROOT}")
        set(OPENSSL_ROOT_DIR "${_EVA_DEFAULT_OPENSSL_ROOT}" CACHE PATH "OpenSSL installation root" FORCE)
    endif()
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
add_subdirectory(thirdparty/cpp-mcp)
include_directories(thirdparty/cpp-mcp/include)
include_directories(thirdparty/cpp-mcp/common)

if (MCP_SSL AND WIN32)
    set(_EVA_OPENSSL_BIN "")
    if (DEFINED OPENSSL_ROOT_DIR)
        set(_EVA_OPENSSL_BIN "${OPENSSL_ROOT_DIR}/bin")
    endif()
    set(_EVA_OPENSSL_DLLS libssl-3-x64.dll libcrypto-3-x64.dll)
    set(_EVA_OPENSSL_COPY_COMMANDS)
    foreach(_dll IN LISTS _EVA_OPENSSL_DLLS)
        if (NOT _EVA_OPENSSL_BIN STREQUAL "" AND EXISTS "${_EVA_OPENSSL_BIN}/${_dll}")
            list(APPEND _EVA_OPENSSL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_EVA_OPENSSL_BIN}/${_dll}"
                        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_dll}")
            foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
                string(TOUPPER "${_cfg}" _CFG_UPPER)
                if (DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_CFG_UPPER})
                    list(APPEND _EVA_OPENSSL_COPY_COMMANDS
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_EVA_OPENSSL_BIN}/${_dll}"
                                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_CFG_UPPER}}/${_dll}")
                endif()
            endforeach()
        endif()
    endforeach()
    if (_EVA_OPENSSL_COPY_COMMANDS)
        add_custom_target(eva_copy_openssl ALL
            ${_EVA_OPENSSL_COPY_COMMANDS}
            COMMENT "Copying OpenSSL runtime libraries for MCP HTTPS support")
    else()
        message(WARNING "OpenSSL runtime libraries not found. Set OPENSSL_ROOT_DIR to a valid OpenSSL 3 installation so MCP HTTPS clients can run.")
    endif()
endif()



