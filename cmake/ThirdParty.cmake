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

#
# Heavier projects (llama.cpp / whisper.cpp / stable-diffusion.cpp)
# are no longer added via add_subdirectory().
# Instead, configure and build them out-of-tree with add_custom_target,
# and copy the required executables into ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}.
# This significantly reduces our CMake graph and avoids polluting targets.
#

# Where to stage external builds
set(EXT_BUILD_DIR ${CMAKE_BINARY_DIR}/3rd)
file(MAKE_DIRECTORY ${EXT_BUILD_DIR})

# --------------------------- llama.cpp ---------------------------
set(LLAMA_SRC ${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp)
set(LLAMA_BLD ${EXT_BUILD_DIR}/llama)
set(LLAMA_BIN ${LLAMA_BLD}/bin)
set(LLAMA_BIN_CFG ${LLAMA_BIN}/$<CONFIG>)

# Configure and build desired llama.cpp tools once
add_custom_target(llama-build
    COMMAND ${CMAKE_COMMAND} -S ${LLAMA_SRC} -B ${LLAMA_BLD}
            -DBUILD_SHARED_LIBS=OFF
            -DLLAMA_CURL=${LLAMA_CURL}
            -DLLAMA_BUILD_TOOLS=ON
            -DLLAMA_BUILD_SERVER=ON
            -DGGML_CUDA=$<BOOL:${GGML_CUDA}>
            -DGGML_VULKAN=$<BOOL:${GGML_VULKAN}>
    COMMAND ${CMAKE_COMMAND} --build ${LLAMA_BLD} --target llama-server llama-quantize llama-tts mtmd --config $<CONFIG>
    BYPRODUCTS
        ${LLAMA_BIN}/llama-server${sfx_NAME}
        ${LLAMA_BIN}/llama-quantize${sfx_NAME}
        ${LLAMA_BIN}/llama-tts${sfx_NAME}
    COMMENT "Building llama.cpp tools (server/quantize/tts/mtmd) out-of-tree"
)

set(LLAMA_SERVER_EXE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/llama-server${sfx_NAME})
set(LLAMA_QUANTIZE_EXE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/llama-quantize${sfx_NAME})
set(LLAMA_TTS_EXE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/llama-tts${sfx_NAME})

add_custom_command(OUTPUT ${LLAMA_SERVER_EXE}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LLAMA_BIN_CFG}/llama-server${sfx_NAME} ${LLAMA_SERVER_EXE}
    DEPENDS llama-build
    COMMENT "Copy llama-server -> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
add_custom_command(OUTPUT ${LLAMA_QUANTIZE_EXE}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LLAMA_BIN_CFG}/llama-quantize${sfx_NAME} ${LLAMA_QUANTIZE_EXE}
    DEPENDS llama-build
    COMMENT "Copy llama-quantize -> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
add_custom_command(OUTPUT ${LLAMA_TTS_EXE}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LLAMA_BIN_CFG}/llama-tts${sfx_NAME} ${LLAMA_TTS_EXE}
    DEPENDS llama-build
    COMMENT "Copy llama-tts -> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

add_custom_target(llama-server   DEPENDS ${LLAMA_SERVER_EXE})
add_custom_target(llama-quantize DEPENDS ${LLAMA_QUANTIZE_EXE})
add_custom_target(llama-tts      DEPENDS ${LLAMA_TTS_EXE})
# mtmd is a library; expose it as imported target for linking (see below)

# Provide headers for mtmd-helper includes (ggml.h / llama.h)
# Note: use target_include_directories in Targets.cmake to wire these to ${EVA_TARGET}
set(LLAMA_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/include
    ${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/ggml/include
    ${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/tools/mtmd
)
set_property(GLOBAL PROPERTY EVA_LLAMA_INCLUDE_DIRS "${LLAMA_INCLUDE_DIRS}")

# Import out-of-tree built static libraries to link with eva
add_library(llama_ext STATIC IMPORTED)
set_target_properties(llama_ext PROPERTIES IMPORTED_LOCATION_RELEASE "${LLAMA_BLD}/src/Release/llama.lib")
add_dependencies(llama_ext llama-build)

add_library(common_ext STATIC IMPORTED)
set_target_properties(common_ext PROPERTIES IMPORTED_LOCATION_RELEASE "${LLAMA_BLD}/common/Release/common.lib")
add_dependencies(common_ext llama-build)

add_library(ggml_ext STATIC IMPORTED)
set_target_properties(ggml_ext PROPERTIES IMPORTED_LOCATION_RELEASE "${LLAMA_BLD}/ggml/src/Release/ggml.lib")
add_dependencies(ggml_ext llama-build)

add_library(mtmd_ext STATIC IMPORTED)
set_target_properties(mtmd_ext PROPERTIES IMPORTED_LOCATION_RELEASE "${LLAMA_BLD}/tools/mtmd/Release/mtmd.lib")
add_dependencies(mtmd_ext llama-build)

# ------------------------- whisper.cpp -------------------------
set(WHISPER_SRC ${CMAKE_SOURCE_DIR}/thirdparty/whisper.cpp)
set(WHISPER_BLD ${EXT_BUILD_DIR}/whisper)
set(WHISPER_BIN ${WHISPER_BLD}/bin)
set(WHISPER_BIN_CFG ${WHISPER_BIN}/$<CONFIG>)

add_custom_target(whisper-build
    COMMAND ${CMAKE_COMMAND} -S ${WHISPER_SRC} -B ${WHISPER_BLD}
            -DBUILD_SHARED_LIBS=OFF
            -DGGML_CUDA=$<BOOL:${GGML_CUDA}>
            -DGGML_VULKAN=$<BOOL:${GGML_VULKAN}>
    COMMAND ${CMAKE_COMMAND} --build ${WHISPER_BLD} --target whisper-cli --config $<CONFIG>
    BYPRODUCTS ${WHISPER_BIN}/whisper-cli${sfx_NAME}
    COMMENT "Building whisper.cpp (whisper-cli) out-of-tree"
)

set(WHISPER_EXE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/whisper-cli${sfx_NAME})
add_custom_command(OUTPUT ${WHISPER_EXE}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WHISPER_BIN_CFG}/whisper-cli${sfx_NAME} ${WHISPER_EXE}
    DEPENDS whisper-build
    COMMENT "Copy whisper-cli -> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
add_custom_target(whisper-cli DEPENDS ${WHISPER_EXE})

# ------------------- stable-diffusion.cpp --------------------
set(SD_SRC ${CMAKE_SOURCE_DIR}/thirdparty/stable-diffusion.cpp)
set(SD_BLD ${EXT_BUILD_DIR}/stable-diffusion)
set(SD_BIN ${SD_BLD}/bin)
set(SD_BIN_CFG ${SD_BIN}/$<CONFIG>)

set(SD_EXTRA_ARGS "")
if (GGML_CUDA)
    list(APPEND SD_EXTRA_ARGS -DSD_USE_CUBLAS=ON)
endif()
if (GGML_VULKAN)
    list(APPEND SD_EXTRA_ARGS -DSD_USE_VULKAN=ON)
endif()

add_custom_target(sd-build
    COMMAND ${CMAKE_COMMAND} -S ${SD_SRC} -B ${SD_BLD} -DBUILD_SHARED_LIBS=OFF ${SD_EXTRA_ARGS}
    COMMAND ${CMAKE_COMMAND} --build ${SD_BLD} --target sd --config $<CONFIG>
    BYPRODUCTS ${SD_BIN}/sd${sfx_NAME}
    COMMENT "Building stable-diffusion.cpp (sd) out-of-tree"
)

set(SD_EXE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sd${sfx_NAME})
add_custom_command(OUTPUT ${SD_EXE}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SD_BIN_CFG}/sd${sfx_NAME} ${SD_EXE}
    DEPENDS sd-build
    COMMENT "Copy sd -> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
add_custom_target(sd DEPENDS ${SD_EXE})

# ------------------------------ MCP ------------------------------
add_subdirectory(thirdparty/cpp-mcp)
include_directories(thirdparty/cpp-mcp/include)
include_directories(thirdparty/cpp-mcp/common)
