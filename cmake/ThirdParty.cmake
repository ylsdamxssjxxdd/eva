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

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_definitions(-DGGML_MAX_NAME=128)
add_definitions(-DGGML_MAX_N_THREADS=512)

###
### Multi-backend out-of-tree builds staged into build/bin/<backend>/
### - Always build CPU backend
### - Optionally build CUDA / Vulkan / OpenCL if requested
###

# Where to stage external builds
set(EXT_BUILD_DIR ${CMAKE_BINARY_DIR}/3rd)
file(MAKE_DIRECTORY ${EXT_BUILD_DIR})

set(LLAMA_SRC   ${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp)
set(WHISPER_SRC ${CMAKE_SOURCE_DIR}/thirdparty/whisper.cpp)
set(SD_SRC      ${CMAKE_SOURCE_DIR}/thirdparty/stable-diffusion.cpp)

set(BACKENDS cpu)
if (GGML_CUDA)
    list(APPEND BACKENDS cuda)
endif()
if (GGML_VULKAN)
    list(APPEND BACKENDS vulkan)
endif()
if (GGML_OPENCL)
    list(APPEND BACKENDS opencl)
endif()

set(ALL_STAGE_TARGETS)

foreach(B IN LISTS BACKENDS)
    string(TOLOWER "${B}" BLOW)
    set(DEST_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BLOW})
    file(MAKE_DIRECTORY ${DEST_DIR})

    # Backend toggles per project
    set(B_USE_CUDA OFF)
    set(B_USE_VULKAN OFF)
    set(B_USE_OPENCL OFF)
    if (BLOW STREQUAL "cuda")
        set(B_USE_CUDA ON)
    elseif(BLOW STREQUAL "vulkan")
        set(B_USE_VULKAN ON)
    elseif(BLOW STREQUAL "opencl")
        set(B_USE_OPENCL ON)
    endif()

    # ---- llama.cpp tools ----
    set(LLAMA_BLD ${EXT_BUILD_DIR}/llama-${BLOW})
    set(LLAMA_BIN ${LLAMA_BLD}/bin)
    set(LLAMA_BIN_CFG ${LLAMA_BIN}/$<CONFIG>)
    add_custom_target(llama-build-${BLOW}
        COMMAND ${CMAKE_COMMAND} -S ${LLAMA_SRC} -B ${LLAMA_BLD}
                -DBUILD_SHARED_LIBS=OFF
                -DLLAMA_CURL=${LLAMA_CURL}
                -DLLAMA_BUILD_TOOLS=ON
                -DLLAMA_BUILD_SERVER=ON
                -DGGML_CUDA=$<BOOL:${B_USE_CUDA}>
                -DGGML_VULKAN=$<BOOL:${B_USE_VULKAN}>
                -DGGML_OPENCL=$<BOOL:${B_USE_OPENCL}>
        COMMAND ${CMAKE_COMMAND} --build ${LLAMA_BLD} --target llama-server llama-quantize llama-tts --config $<CONFIG>
        BYPRODUCTS
            ${LLAMA_BIN}/llama-server${sfx_NAME}
            ${LLAMA_BIN}/llama-quantize${sfx_NAME}
            ${LLAMA_BIN}/llama-tts${sfx_NAME}
        COMMENT "Building llama.cpp (${BLOW})"
    )
    add_custom_command(OUTPUT ${DEST_DIR}/llama-server${sfx_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LLAMA_BIN_CFG}/llama-server${sfx_NAME} ${DEST_DIR}/llama-server${sfx_NAME}
        DEPENDS llama-build-${BLOW}
        COMMENT "Stage llama-server -> ${DEST_DIR}"
    )
    add_custom_command(OUTPUT ${DEST_DIR}/llama-quantize${sfx_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LLAMA_BIN_CFG}/llama-quantize${sfx_NAME} ${DEST_DIR}/llama-quantize${sfx_NAME}
        DEPENDS llama-build-${BLOW}
        COMMENT "Stage llama-quantize -> ${DEST_DIR}"
    )
    add_custom_command(OUTPUT ${DEST_DIR}/llama-tts${sfx_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LLAMA_BIN_CFG}/llama-tts${sfx_NAME} ${DEST_DIR}/llama-tts${sfx_NAME}
        DEPENDS llama-build-${BLOW}
        COMMENT "Stage llama-tts -> ${DEST_DIR}"
    )
    add_custom_target(stage-llama-${BLOW}
        DEPENDS ${DEST_DIR}/llama-server${sfx_NAME} ${DEST_DIR}/llama-quantize${sfx_NAME} ${DEST_DIR}/llama-tts${sfx_NAME}
    )

    # ---- whisper.cpp ----
    set(WHISPER_BLD ${EXT_BUILD_DIR}/whisper-${BLOW})
    set(WHISPER_BIN ${WHISPER_BLD}/bin)
    set(WHISPER_BIN_CFG ${WHISPER_BIN}/$<CONFIG>)
    add_custom_target(whisper-build-${BLOW}
        COMMAND ${CMAKE_COMMAND} -S ${WHISPER_SRC} -B ${WHISPER_BLD}
                -DBUILD_SHARED_LIBS=OFF
                -DGGML_CUDA=$<BOOL:${B_USE_CUDA}>
                -DGGML_VULKAN=$<BOOL:${B_USE_VULKAN}>
                -DGGML_OPENCL=$<BOOL:${B_USE_OPENCL}>
        COMMAND ${CMAKE_COMMAND} --build ${WHISPER_BLD} --target whisper-cli --config $<CONFIG>
        BYPRODUCTS ${WHISPER_BIN}/whisper-cli${sfx_NAME}
        COMMENT "Building whisper.cpp (${BLOW})"
    )
    add_custom_command(OUTPUT ${DEST_DIR}/whisper-cli${sfx_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WHISPER_BIN_CFG}/whisper-cli${sfx_NAME} ${DEST_DIR}/whisper-cli${sfx_NAME}
        DEPENDS whisper-build-${BLOW}
        COMMENT "Stage whisper-cli -> ${DEST_DIR}"
    )
    add_custom_target(stage-whisper-${BLOW}
        DEPENDS ${DEST_DIR}/whisper-cli${sfx_NAME}
    )

    # ---- stable-diffusion.cpp ----
    set(SD_BLD ${EXT_BUILD_DIR}/stable-diffusion-${BLOW})
    set(SD_BIN ${SD_BLD}/bin)
    set(SD_BIN_CFG ${SD_BIN}/$<CONFIG>)
    # Configure stable-diffusion.cpp backend toggles
    # Note: upstream expects SD_CUDA / SD_VULKAN / SD_OPENCL options.
    # We previously passed SD_USE_CUBLAS / SD_USE_VULKAN which are not
    # recognized as options by the subproject, resulting in CPU builds.
    set(SD_EXTRA_ARGS "")
    if (B_USE_CUDA)
        list(APPEND SD_EXTRA_ARGS -DSD_CUDA=ON)
    endif()
    if (B_USE_VULKAN)
        list(APPEND SD_EXTRA_ARGS -DSD_VULKAN=ON)
    endif()
    if (B_USE_OPENCL)
        list(APPEND SD_EXTRA_ARGS -DSD_OPENCL=ON)
    endif()
    add_custom_target(sd-build-${BLOW}
        COMMAND ${CMAKE_COMMAND} -S ${SD_SRC} -B ${SD_BLD} -DBUILD_SHARED_LIBS=OFF ${SD_EXTRA_ARGS}
        COMMAND ${CMAKE_COMMAND} --build ${SD_BLD} --target sd --config $<CONFIG>
        BYPRODUCTS ${SD_BIN}/sd${sfx_NAME}
        COMMENT "Building stable-diffusion.cpp (${BLOW})"
    )
    add_custom_command(OUTPUT ${DEST_DIR}/sd${sfx_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SD_BIN_CFG}/sd${sfx_NAME} ${DEST_DIR}/sd${sfx_NAME}
        DEPENDS sd-build-${BLOW}
        COMMENT "Stage sd -> ${DEST_DIR}"
    )
    add_custom_target(stage-sd-${BLOW}
        DEPENDS ${DEST_DIR}/sd${sfx_NAME}
    )

    # Collect staged targets
    list(APPEND ALL_STAGE_TARGETS stage-llama-${BLOW} stage-whisper-${BLOW} stage-sd-${BLOW})
endforeach()

# Single umbrella target that ensures all selected backends are built/staged
add_custom_target(backends ALL DEPENDS ${ALL_STAGE_TARGETS})


# ------------------------------ MCP ------------------------------
add_subdirectory(thirdparty/cpp-mcp)
include_directories(thirdparty/cpp-mcp/include)
include_directories(thirdparty/cpp-mcp/common)
