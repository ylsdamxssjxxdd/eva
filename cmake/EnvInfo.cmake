# EnvInfo.cmake - compute output name, environment string, and generate cmakeconfig.h

# Prevent double-include
include_guard(GLOBAL)

# Normalize ARCHITECTURE display string
set(ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
if (ARCHITECTURE MATCHES "x86_64|AMD64|i[3-6]86")
    set(ARCHITECTURE "x86")
elseif (ARCHITECTURE MATCHES "aarch64|arm")
    set(ARCHITECTURE "arm")
endif()

# Compose eva output name
set(eva_OUTPUT_NAME "${EVA_TARGET}-${version}-${ARCHITECTURE}")

# OS version
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(COMMAND lsb_release -d OUTPUT_VARIABLE OS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    execute_process(COMMAND sw_vers -productVersion OUTPUT_VARIABLE OS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    execute_process(COMMAND cmd.exe /C ver OUTPUT_VARIABLE OS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX REPLACE "\r" "" OS_VERSION "${OS_VERSION}")
    string(STRIP "${OS_VERSION}" OS_VERSION)
else()
    set(OS_VERSION "unknown OS")
endif()

set(PROCESSOR_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
string(CONCAT eva_ENVIRONMENT "${OS_VERSION} CPU: ${PROCESSOR_ARCHITECTURE}")

# GPU detection best-effort
set(GPU_ARCH "")
find_program(NVIDIA_SMI nvidia-smi)
if (NVIDIA_SMI)
    execute_process(COMMAND ${NVIDIA_SMI} --query-gpu=gpu_name --format=csv,noheader
        OUTPUT_VARIABLE NVIDIA_GPU_OUTPUT ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NVIDIA_GPU_OUTPUT)
        string(REGEX REPLACE "\n.*" "" FIRST_GPU_LINE "${NVIDIA_GPU_OUTPUT}")
        if   (FIRST_GPU_LINE MATCHES "V100")
            set(GPU_ARCH "NVIDIA Volta")
        elseif(FIRST_GPU_LINE MATCHES "A100|RTX 3")
            set(GPU_ARCH "NVIDIA Ampere")
        elseif(FIRST_GPU_LINE MATCHES "RTX 2")
            set(GPU_ARCH "NVIDIA Turing")
        else()
            set(GPU_ARCH "NVIDIA ${FIRST_GPU_LINE}")
        endif()
    endif()
endif()

if (NOT GPU_ARCH)
    find_program(ROCK_SMI rocm-smi)
    if (ROCK_SMI)
        execute_process(COMMAND ${ROCK_SMI} --showid OUTPUT_VARIABLE AMD_GPU_OUTPUT ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (AMD_GPU_OUTPUT MATCHES "Device Name: (.*)")
            set(AMD_GPU_MODEL "${CMAKE_MATCH_1}")
            if (AMD_GPU_MODEL MATCHES "Radeon RX 7")
                set(GPU_ARCH "AMD RDNA3")
            else()
                set(GPU_ARCH "AMD ${AMD_GPU_MODEL}")
            endif()
        endif()
    endif()
endif()

if (NOT GPU_ARCH)
    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        execute_process(COMMAND system_profiler SPDisplaysDataType OUTPUT_VARIABLE MAC_GPU_OUTPUT ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (MAC_GPU_OUTPUT MATCHES "Chipset Model: (Apple [^\n]+)")
            set(GPU_ARCH "${CMAKE_MATCH_1}")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Note: escape inner single quotes for PowerShell here-string using doubled quotes
        execute_process(COMMAND bash -lc "lspci -nn | grep -i 'VGA compatible controller'" OUTPUT_VARIABLE PCI_GPU_OUTPUT ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (PCI_GPU_OUTPUT MATCHES "Intel Corporation (.*)\\[")
            set(GPU_ARCH "Intel ${CMAKE_MATCH_1}")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        execute_process(COMMAND powershell -Command "(Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name)" OUTPUT_VARIABLE WIN_GPU_OUTPUT ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(STRIP "${WIN_GPU_OUTPUT}" WIN_GPU_OUTPUT)
        if (WIN_GPU_OUTPUT MATCHES "NVIDIA|AMD|Intel")
            set(GPU_ARCH "${WIN_GPU_OUTPUT}")
        endif()
    endif()
endif()

if (GPU_ARCH)
    string(CONCAT eva_ENVIRONMENT "${eva_ENVIRONMENT}, GPU: ${GPU_ARCH}")
else()
    string(CONCAT eva_ENVIRONMENT "${eva_ENVIRONMENT}, GPU: unknown")
endif()
message(STATUS "生产环境: ${eva_ENVIRONMENT}")

# Expose variables and generate header
set(EVA_ENVIRONMENT ${eva_ENVIRONMENT})
set(EVA_VERSION ${eva_OUTPUT_NAME})
set(COMPILE_VERSION "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if (DEFINED Qt5Core_VERSION)
    set(QT_VERSION_ ${Qt5Core_VERSION})
elseif(DEFINED Qt5Widgets_VERSION)
    set(QT_VERSION_ ${Qt5Widgets_VERSION})
endif()
if (NOT DEFINED EVA_PRODUCT_TIME)
    string(TIMESTAMP EVA_PRODUCT_TIME "%Y-%m-%d %H:%M:%S")
    set(EVA_PRODUCT_TIME "${EVA_PRODUCT_TIME}" CACHE STRING "Product build time (frozen across re-configures)")
endif()

# Keep generated header at build/src/utils so includes like "./src/utils/cmakeconfig.h" work
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/src/utils)
configure_file("${CMAKE_SOURCE_DIR}/src/utils/cmakeconfig.h.in" "${CMAKE_BINARY_DIR}/src/utils/cmakeconfig.h")


