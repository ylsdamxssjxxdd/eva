# ThirdParty.cmake - bring in and configure external deps

# Prevent double-include
include_guard(GLOBAL)

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

add_subdirectory(thirdparty/llama.cpp/ggml)

# whisper.cpp-34972db
add_subdirectory(thirdparty/whisper.cpp)
add_subdirectory(thirdparty/whisper.cpp/examples)

# stable-diffusion.cpp-4570715
add_subdirectory(thirdparty/stable-diffusion.cpp)
add_subdirectory(thirdparty/stable-diffusion.cpp/examples)

add_subdirectory(thirdparty/llama.cpp)
add_subdirectory(thirdparty/llama.cpp/common)
set_target_properties(llama PROPERTIES PUBLIC_HEADER thirdparty/llama.cpp/include/llama.h)
if (UNIX AND NOT APPLE)
    set_target_properties(llama PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()
# add_subdirectory(thirdparty/llama.cpp/tools/imatrix)
# add_subdirectory(thirdparty/llama.cpp/tools/main)
add_subdirectory(thirdparty/llama.cpp/tools/mtmd)
add_subdirectory(thirdparty/llama.cpp/tools/tts)
add_subdirectory(thirdparty/llama.cpp/tools/quantize)
add_subdirectory(thirdparty/llama.cpp/tools/server)

# MCP
add_subdirectory(thirdparty/cpp-mcp)
include_directories(thirdparty/cpp-mcp/include)
include_directories(thirdparty/cpp-mcp/common)
