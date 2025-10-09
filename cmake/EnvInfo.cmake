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

string(CONCAT eva_ENVIRONMENT "${eva_ENVIRONMENT}, GPU: unknown")
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


