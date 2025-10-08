# ProjectOptions.cmake - centralize build options and platform flags

# Prevent double-include
include_guard(GLOBAL)

# ---- User-facing options ----
option(BODY_PACK   "pack eva"                                   OFF)

# Backend toggles (kept for compatibility; not used to probe SDKs)
option(GGML_CUDA   "ggml: use CUDA"                             OFF)
option(GGML_VULKAN "ggml: use Vulkan"                           OFF)
option(GGML_OPENCL "ggml: use OpenCL"                           OFF)
option(GGML_BACKEND_AUTO "auto-detect CUDA/Vulkan/OpenCL SDKs (disabled)" OFF)
option(BODY_32BIT  "support 32 BIT"                             OFF)
option(BODY_DOTPORD "使用常规arm dotprod加速"                    OFF)

# ---- Global toggles that affect subprojects ----
option(BUILD_SHARED_LIBS         "build shared libraries" ON)
option(SD_BUILD_SHARED_LIBS      "sd: build shared libs"   ON)
option(LLAMA_CURL                "llama: use libcurl to download model from an URL" OFF)
option(MCP_SSL                   "Enable SSL support" OFF)

add_compile_definitions(_WIN32_WINNT=0x0601)
set(GGML_WIN_VER "0x601" CACHE STRING "ggml: Windows version")

# ---- Acceleration mode for the main app ----
if (BODY_32BIT)
    # 32-bit Windows 7 support requires MinGW; disable CPU/GPU opt flags
    if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Windows"))
        message(FATAL_ERROR "This project requires MinGW for 32-bit build.")
    endif()
    message(STATUS "32bit 构建：关闭高阶 CPU 指令以兼容 Win7")
    set(GGML_NATIVE OFF)
    option(GGML_FMA          "ggml: enable FMA"              OFF)
    option(GGML_F16C         "ggml: enable F16C"             OFF)
    option(GGML_AVX          "ggml: enable AVX"              OFF)
    option(GGML_AVX2         "ggml: enable AVX2"             OFF)
    option(GGML_BMI2         "ggml: enable BMI2"             OFF)
    option(GGML_CPU_AARCH64  "ggml: use runtime weight conversion of Q4_0 to Q4_X_X" OFF)
    add_compile_definitions(BODY_USE_32BIT)
else()
    # CPU default; no SDK probing and no GPU defines from CMake
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
    # Silence MSVC STL deprecation warning for checked array iterators
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
elseif (MINGW)
    add_compile_definitions(_XOPEN_SOURCE=600)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -Wall -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s")
elseif (UNIX)
    message(STATUS "Compiling on Unix/Linux")
    find_package(X11 REQUIRED)
    list(APPEND extra_LIBS X11::Xtst)
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm" AND NOT BODY_DOTPORD)
    message(STATUS "ARM 架构：建议使用 clang 以获得更好优化")
    set(GGML_NATIVE OFF)
endif()

if (NOT DEFINED GGML_LLAMAFILE)
    set(GGML_LLAMAFILE_DEFAULT ON)
endif()
if (NOT DEFINED GGML_CUDA_GRAPHS)
    set(GGML_CUDA_GRAPHS_DEFAULT ON)
endif()

# ---- Language standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
