# ProjectOptions.cmake - centralize build options and platform flags

# Prevent double-include
include_guard(GLOBAL)

# ---- User-facing options ----
option(BODY_PACK   "pack eva"                                   OFF)
option(EVA_ENABLE_COVERAGE "Enable gcov/llvm-cov instrumentation for coverage reports" OFF)

if(EVA_ENABLE_COVERAGE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "EVA: coverage instrumentation is enabled (compiler: ${CMAKE_CXX_COMPILER_ID}).")
        foreach(_lang C CXX)
            set(CMAKE_${_lang}_FLAGS "${CMAKE_${_lang}_FLAGS} --coverage -O0 -g")
            set(CMAKE_${_lang}_FLAGS_RELEASE "${CMAKE_${_lang}_FLAGS_RELEASE} --coverage -O0 -g")
            set(CMAKE_${_lang}_FLAGS_RELWITHDEBINFO "${CMAKE_${_lang}_FLAGS_RELWITHDEBINFO} --coverage -O0 -g")
            set(CMAKE_${_lang}_FLAGS_DEBUG "${CMAKE_${_lang}_FLAGS_DEBUG} --coverage -O0 -g")
        endforeach()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
        add_compile_definitions(EVA_COVERAGE_BUILD)
    else()
        message(WARNING "EVA_ENABLE_COVERAGE currently supports GCC/Clang toolchains only. Option ignored.")
        set(EVA_ENABLE_COVERAGE OFF CACHE BOOL "Enable gcov/llvm-cov instrumentation for coverage reports" FORCE)
    endif()
endif()

# ---- Dependency hint helpers ----
set(EVA_OPENSSL_FALLBACK_DIR "C:/openssl-static-3.1.1" CACHE PATH "Fallback OpenSSL root when OPENSSL_ROOT_DIR/OPENSSL_PREFIX is missing")
set(_EVA_OPENSSL_ROOT "")
if(DEFINED OPENSSL_ROOT_DIR AND NOT OPENSSL_ROOT_DIR STREQUAL "")
    set(_EVA_OPENSSL_ROOT "${OPENSSL_ROOT_DIR}")
elseif(DEFINED ENV{OPENSSL_PREFIX} AND NOT "$ENV{OPENSSL_PREFIX}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{OPENSSL_PREFIX}" _EVA_OPENSSL_ROOT)
endif()
# 未设置/无效时自动回退到默认路径。
if(_EVA_OPENSSL_ROOT STREQUAL "" OR NOT EXISTS "${_EVA_OPENSSL_ROOT}")
    if(EXISTS "${EVA_OPENSSL_FALLBACK_DIR}")
        message(STATUS "EVA: OPENSSL_ROOT_DIR/OPENSSL_PREFIX invalid, fallback to ${EVA_OPENSSL_FALLBACK_DIR}.")
        set(_EVA_OPENSSL_ROOT "${EVA_OPENSSL_FALLBACK_DIR}")
    endif()
endif()
if(EXISTS "${_EVA_OPENSSL_ROOT}")
    if(NOT DEFINED OPENSSL_ROOT_DIR OR OPENSSL_ROOT_DIR STREQUAL "" OR NOT EXISTS "${OPENSSL_ROOT_DIR}")
        set(OPENSSL_ROOT_DIR "${_EVA_OPENSSL_ROOT}" CACHE PATH "OpenSSL root derived from OPENSSL_ROOT_DIR/OPENSSL_PREFIX or fallback" FORCE)
    endif()
    if(NOT OPENSSL_ROOT_DIR IN_LIST CMAKE_PREFIX_PATH)
        list(APPEND CMAKE_PREFIX_PATH "${OPENSSL_ROOT_DIR}")
    endif()
    if(NOT "${OPENSSL_ROOT_DIR}/include" IN_LIST CMAKE_INCLUDE_PATH AND EXISTS "${OPENSSL_ROOT_DIR}/include")
        list(APPEND CMAKE_INCLUDE_PATH "${OPENSSL_ROOT_DIR}/include")
    endif()
    if(EXISTS "${OPENSSL_ROOT_DIR}/lib64" AND NOT "${OPENSSL_ROOT_DIR}/lib64" IN_LIST CMAKE_LIBRARY_PATH)
        list(APPEND CMAKE_LIBRARY_PATH "${OPENSSL_ROOT_DIR}/lib64")
    endif()
    if(EXISTS "${OPENSSL_ROOT_DIR}/lib" AND NOT "${OPENSSL_ROOT_DIR}/lib" IN_LIST CMAKE_LIBRARY_PATH)
        list(APPEND CMAKE_LIBRARY_PATH "${OPENSSL_ROOT_DIR}/lib")
    endif()
    message(STATUS "EVA: Using OpenSSL from ${OPENSSL_ROOT_DIR} (root or fallback)")
elseif(DEFINED ENV{OPENSSL_PREFIX} AND NOT "$ENV{OPENSSL_PREFIX}" STREQUAL "")
    message(WARNING "EVA: OPENSSL_PREFIX=${_EVA_OPENSSL_ROOT}, but the path does not exist. Ignoring.")
endif()
unset(_EVA_OPENSSL_ROOT)

# 在 find_package(Qt5) 之前修补 Qt 元数据里的 OpenSSL 路径。
function(eva_patch_qt_openssl_prl)
    if(NOT WIN32)
        return()
    endif()
    if(NOT DEFINED EVA_OPENSSL_FALLBACK_DIR OR EVA_OPENSSL_FALLBACK_DIR STREQUAL "")
        return()
    endif()
    file(TO_CMAKE_PATH "${EVA_OPENSSL_FALLBACK_DIR}" _eva_fallback_root)
    if(NOT EXISTS "${_eva_fallback_root}/lib/libssl.a" OR NOT EXISTS "${_eva_fallback_root}/lib/libcrypto.a")
        return()
    endif()

    set(_eva_qt_prefix "")
    if(DEFINED Qt5_DIR AND EXISTS "${Qt5_DIR}/Qt5Config.cmake")
        get_filename_component(_eva_qt_prefix "${Qt5_DIR}/../../../" ABSOLUTE)
    elseif(DEFINED ENV{Qt5_DIR} AND EXISTS "$ENV{Qt5_DIR}/Qt5Config.cmake")
        get_filename_component(_eva_qt_prefix "$ENV{Qt5_DIR}/../../../" ABSOLUTE)
    elseif(DEFINED ENV{QTDIR} AND EXISTS "$ENV{QTDIR}")
        file(TO_CMAKE_PATH "$ENV{QTDIR}" _eva_qt_prefix)
    else()
        foreach(_eva_prefix IN LISTS CMAKE_PREFIX_PATH)
            if(EXISTS "${_eva_prefix}/lib/cmake/Qt5/Qt5Config.cmake")
                set(_eva_qt_prefix "${_eva_prefix}")
                break()
            endif()
        endforeach()
    endif()
    if(_eva_qt_prefix STREQUAL "")
        return()
    endif()

    file(GLOB_RECURSE _eva_qt_meta_files
        "${_eva_qt_prefix}/*.prl"
        "${_eva_qt_prefix}/*.pri"
        "${_eva_qt_prefix}/*.pc")
    if(NOT _eva_qt_meta_files)
        return()
    endif()

    set(_eva_patched_count 0)
    foreach(_eva_meta_path IN LISTS _eva_qt_meta_files)
        file(READ "${_eva_meta_path}" _eva_meta_text)
        if(NOT _eva_meta_text MATCHES "openssl-static-3\\.1\\.1")
            continue()
        endif()
        set(_eva_updated_text "${_eva_meta_text}")
        string(REPLACE "\\" "/" _eva_updated_text "${_eva_updated_text}")
        string(REGEX REPLACE "/+" "/" _eva_updated_text "${_eva_updated_text}")
        string(REGEX REPLACE "[A-Za-z]:/openssl-static-3\\.1\\.1" "${_eva_fallback_root}" _eva_updated_text "${_eva_updated_text}")
        if(NOT _eva_updated_text STREQUAL _eva_meta_text)
            file(WRITE "${_eva_meta_path}" "${_eva_updated_text}")
            math(EXPR _eva_patched_count "${_eva_patched_count} + 1")
        endif()
    endforeach()
    if(_eva_patched_count GREATER 0)
        message(STATUS "EVA: patched ${_eva_patched_count} Qt OpenSSL metadata paths to ${_eva_fallback_root}.")
    endif()
endfunction()

eva_patch_qt_openssl_prl()

# MinGW static runtime option (mirrors the pattern in the reference CMake)
# Default: Windows ON, others OFF. This aims to statically link libgcc/libstdc++/winpthread
# while keeping Qt dynamic unless you provide static Qt yourself.
if (WIN32)
    set(DEFAULT_EVA_STATIC ON)
else()
    set(DEFAULT_EVA_STATIC OFF)
endif()
option(EVA_STATIC  "MinGW: static libgcc/libstdc++/winpthread (Qt stays dynamic)" ${DEFAULT_EVA_STATIC})

# MinGW: whether to copy runtime DLLs after build. Default OFF when EVA_STATIC=ON.
if (WIN32 AND MINGW)
    if (EVA_STATIC)
        set(DEFAULT_BODY_COPY_MINGW_RUNTIME OFF)
    else()
        set(DEFAULT_BODY_COPY_MINGW_RUNTIME ON)
    endif()
    option(BODY_COPY_MINGW_RUNTIME "Copy MinGW runtime DLLs after build (libgcc/libstdc++/winpthread)" ${DEFAULT_BODY_COPY_MINGW_RUNTIME})
endif()

# Linux static Qt option
if (UNIX AND NOT APPLE)
    option(EVA_LINUX_STATIC "Link against a static Qt build on Linux (skips AppImage packaging)" OFF)
    if (EVA_LINUX_STATIC)
        if(NOT DEFINED OPENSSL_USE_STATIC_LIBS)
            set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "Prefer static OpenSSL libs when EVA_LINUX_STATIC=ON")
        endif()
        add_compile_definitions(EVA_LINUX_STATIC_BUILD)
    endif()
    set(EVA_FCITX_PLUGIN_PATH "" CACHE FILEPATH "Path to libfcitxplatforminputcontextplugin.a for Linux static builds")
    set(EVA_FCITX_PLUGIN_LIBS "" CACHE STRING "Extra libraries required by fcitx platform plugin when linked statically")
    option(EVA_LINUX_STATIC_SKIP_FLITE "Disable linking the Flite text-to-speech plugin when building statically on Linux" OFF)
    if (EVA_LINUX_STATIC_SKIP_FLITE)
        add_compile_definitions(EVA_SKIP_FLITE_PLUGIN)
    endif()
    set(EVA_TTS_FLITE_PLUGIN_PATH "" CACHE FILEPATH "Path to the static QTextToSpeech flite plugin library")
    set(EVA_TTS_FLITE_PLUGIN_LIBS "" CACHE STRING "Extra libraries required by the flite text-to-speech plugin")
    option(EVA_APPIMAGE_BUNDLE_COMMON_LIBS "Auto-detect and bundle common desktop runtime libraries into the AppImage" ON)
    set(EVA_APPIMAGE_COMMON_LIB_NAMES
        "X11;X11-xcb;Xau;Xdmcp;xcb;SM;ICE;EGL;GL;GLX;GLdispatch;wayland-client;wayland-egl;asound;pulse;pulse-mainloop-glib;uuid;z;bsd;gmp;gpg-error;Xtst;Xext;Xrender;Xi;Xcursor;Xfixes;xkbcommon-x11;xcb-image;xcb-shm;xcb-icccm;xcb-keysyms;xcb-randr;xcb-render;xcb-render-util;xcb-shape;xcb-sync;xcb-xfixes;xcb-xinerama;xcb-xinput;xcb-glx;gstapp-1.0;gstpbutils-1.0;gstaudio-1.0;gstvideo-1.0;gstbase-1.0;gstreamer-1.0;gobject-2.0;glib-2.0;gio-2.0;gmodule-2.0;gthread-2.0;mysqlclient;odbc;pq;tiff;webp;webpdemux;webpmux"
        CACHE STRING "Semicolon-separated library names that will be auto-detected and bundled into the AppImage")
    set(EVA_APPIMAGE_EXTRA_LIBS "" CACHE STRING "Semicolon-separated absolute library paths to bundle into the AppImage (Linux only)")
    set(EVA_APPIMAGE_EXTRA_BINS "" CACHE STRING "Semicolon-separated extra executables to place in the AppImage bin directory (Linux only)")
endif()

set(_EVA_ENABLE_QT_TTS_DEFAULT ON)
if (UNIX AND NOT APPLE AND EVA_LINUX_STATIC)
    set(_EVA_ENABLE_QT_TTS_DEFAULT OFF)
endif()
option(EVA_ENABLE_QT_TTS "Enable Qt TextToSpeech support" ${_EVA_ENABLE_QT_TTS_DEFAULT})
if (UNIX AND NOT APPLE AND EVA_LINUX_STATIC AND EVA_ENABLE_QT_TTS)
    message(STATUS "EVA_LINUX_STATIC detected: forcing EVA_ENABLE_QT_TTS=OFF to drop Qt TextToSpeech dependency.")
    set(EVA_ENABLE_QT_TTS OFF CACHE BOOL "Enable Qt TextToSpeech support" FORCE)
endif()
if (EVA_ENABLE_QT_TTS)
    add_compile_definitions(EVA_ENABLE_QT_TTS)
else()
    add_compile_definitions(EVA_DISABLE_QT_TTS)
endif()

# ---- Global toggles that affect subprojects ----
option(MCP_SSL                   "Enable SSL support" ON)

add_compile_definitions(_WIN32_WINNT=0x0601)

# ---- Packaging specific flags ----
if (BODY_PACK)
    if (WIN32)
        set(BODY_PACK_EXE WIN32)
        add_compile_definitions(BODY_WIN_PACK)
    elseif(UNIX)
        add_compile_definitions(BODY_LINUX_PACK)
    endif()
endif()

# Control whether to run windeployqt during Windows packaging.
# Default: skip for MinGW (static Qt is common there), run for MSVC.
if (WIN32)
    if (MINGW)
        set(DEFAULT_BODY_SKIP_WINDEPLOYQT ON)
    else()
        set(DEFAULT_BODY_SKIP_WINDEPLOYQT OFF)
    endif()
    option(BODY_SKIP_WINDEPLOYQT "Skip running windeployqt when BODY_PACK=ON (useful for static Qt)" ${DEFAULT_BODY_SKIP_WINDEPLOYQT})
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

    # Collect target-scoped options to apply after the executable is created
    set(EVA_COMPILE_OPTIONS "")
    set(EVA_LINK_OPTIONS    "")

    if (EVA_STATIC)
        # Keep Qt dynamic; only make the MinGW runtime static to simplify deployment
        list(APPEND EVA_COMPILE_OPTIONS
            -fno-keep-inline-dllexport
            -fopenmp
            -O2
                        -Wall -Wextra
            -ffunction-sections -fdata-sections
            -fexceptions
            -mthreads)
        list(APPEND EVA_LINK_OPTIONS
            -static
            -static-libgcc
            -static-libstdc++
            -fopenmp   # 交给编译器自己决定链接静态还是动态 libgomp
            -Wl,-s
            -Wl,--gc-sections
            -mthreads
            -lpthread)
    else()
        list(APPEND EVA_COMPILE_OPTIONS
            -fopenmp
            -O2
            -Wall -Wextra
            -ffunction-sections -fdata-sections
            -fexceptions
            -mthreads)
        list(APPEND EVA_LINK_OPTIONS
            -Wl,--gc-sections
            -mthreads
            -lpthread)
    endif()
elseif (UNIX)
    message(STATUS "Compiling on Unix/Linux")
    find_package(X11 REQUIRED)
    list(APPEND extra_LIBS X11::Xtst)
endif()

# ---- Language standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

