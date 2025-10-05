# ProjectOptions.cmake - centralize build options and platform flags

# Prevent double-include
include_guard(GLOBAL)

# ---- User-facing options ----
option(BODY_PACK   "pack eva"                                   OFF)
option(GGML_CUDA   "ggml: use CUDA"                             ON)
option(GGML_VULKAN "ggml: use Vulkan"                           OFF)
option(BODY_32BIT  "support 32 BIT"                             OFF)
option(BODY_DOTPORD "使用常规arm dotprod加速"                    OFF)

# ---- Global toggles that affect subprojects ----
option(BUILD_SHARED_LIBS         "build shared libraries" ON)
option(SD_BUILD_SHARED_LIBS      "sd: build shared libs"   ON)
option(LLAMA_CURL                "llama: use libcurl to download model from an URL" OFF)
option(MCP_SSL                   "Enable SSL support" OFF)

add_compile_definitions(_WIN32_WINNT=0x0601)
set(GGML_WIN_VER "0x601" CACHE STRING "ggml: Windows version")

# ---- Mutually exclusive acceleration modes ----
if (BODY_32BIT)
    # 32-bit Windows 7 support requires MinGW; disable CPU/GPU opt flags
    if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Windows"))
        message(FATAL_ERROR "This project requires MinGW for 32-bit build.")
    endif()
    message(STATUS "32bit关闭所有加速")
    set(GGML_NATIVE OFF)
    option(GGML_FMA          "ggml: enable FMA"              OFF)
    option(GGML_F16C         "ggml: enable F16C"             OFF)
    option(GGML_AVX          "ggml: enable AVX"              OFF)
    option(GGML_AVX2         "ggml: enable AVX2"             OFF)
    option(GGML_BMI2         "ggml: enable BMI2"             OFF)
    option(GGML_CPU_AARCH64  "ggml: use runtime weight conversion of Q4_0 to Q4_X_X" OFF)
    add_compile_definitions(BODY_USE_32BIT)
elseif (GGML_CUDA)
    set(GGML_NATIVE OFF)
    add_compile_definitions(BODY_USE_CUDA)
    add_compile_definitions(BODY_USE_GPU)
    add_compile_definitions(GGML_USE_CUDA)
    add_definitions(-DSD_USE_CUBLAS)
elseif (GGML_VULKAN)
    find_package(Vulkan)
    add_compile_definitions(BODY_USE_VULKAN)
    add_compile_definitions(BODY_USE_GPU)
    add_definitions(-DSD_USE_VULKAN)
else()
    # CPU default
endif()

# ---- Packaging specific flags ----
if (BODY_PACK)
    if (WIN32)
        set(BODY_PACK_EXE WIN32)
        add_compile_definitions(BODY_WIN_PACK)
    elseif(UNIX)
        add_compile_definitions(BODY_LINUX_PACK)
    endif()
endif()

# ---- Compiler & platform flags ----
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /DNOMINMAX /DWIN32_LEAN_AND_MEAN")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /utf-8")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
    # Silence MSVC STL deprecation warning STL4043 for checked array iterators
    # These warnings originate in MS headers and aren't actionable in this project; see build logs
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    if (GGML_CUDA)
        list(APPEND extra_INCLUDES ${CMAKE_SOURCE_DIR}/src/utils/nvml.h)
        list(APPEND extra_LIBS ${CMAKE_SOURCE_DIR}/src/utils/nvml.lib)
        find_package(CUDAToolkit)
        string(REGEX MATCH "^[0-9]+" CUDA_VERSION_MAJOR ${CUDAToolkit_VERSION})
        string(REGEX MATCH "^[0-9]+\\.[0-9]+" CUDA_VERSION ${CUDAToolkit_VERSION})
        message(STATUS "cuda主版本 ${CUDA_VERSION_MAJOR}")
        message(STATUS "cuda库路径 ${CUDAToolkit_BIN_DIR}")
    endif()
elseif (MINGW)
    add_compile_definitions(_XOPEN_SOURCE=600)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -Wall -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s")
elseif (UNIX)
    message(STATUS "Compiling on Unix/Linux")
    find_package(X11 REQUIRED)
    list(APPEND extra_LIBS X11::Xtst)
    if (GGML_CUDA)
        list(APPEND extra_INCLUDES ${CMAKE_SOURCE_DIR}/src/utils/gpuchecker.h)
        find_package(CUDAToolkit)
        string(REGEX MATCH "^[0-9]+" CUDA_VERSION_MAJOR ${CUDAToolkit_VERSION})
        string(REGEX MATCH "^[0-9]+\\.[0-9]+" CUDA_VERSION ${CUDAToolkit_VERSION})
        message(STATUS "cuda主版本 ${CUDA_VERSION_MAJOR}")
        message(STATUS "cuda库路径 ${CUDAToolkit_BIN_DIR}")
    endif()
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm" AND NOT BODY_DOTPORD)
    message(STATUS "arm下关闭所有cpu加速，使用clang编译器可提高一定速度")
    set(GGML_NATIVE OFF)
endif()

if (NOT DEFINED GGML_LLAMAFILE)
    set(GGML_LLAMAFILE_DEFAULT ON) # 默认开启LLAMAFILE加速
endif()
if (NOT DEFINED GGML_CUDA_GRAPHS)
    set(GGML_CUDA_GRAPHS_DEFAULT ON) # 默认开启CUDA_GRAPHS加速
endif()
# ---- Language standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
