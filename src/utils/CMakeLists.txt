########################## 构建生成程序的名字 ################################
set(ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})# 获取处理器架构信息
# 根据不同架构简化处理
if(ARCHITECTURE MATCHES "x86_64|AMD64|i[3-6]86")
    set(ARCHITECTURE "x86")
elseif(ARCHITECTURE MATCHES "aarch64|arm")
    set(ARCHITECTURE "arm")
endif()

if(BODY_32BIT)
    set(eva_OUTPUT_NAME "${TARGET}-${version}-cpu-32bit")
elseif(GGML_VULKAN)
    set(eva_OUTPUT_NAME "${TARGET}-${version}-vulkan-${ARCHITECTURE}")
elseif(GGML_CUDA)
    message(STATUS "CUDA version: ${CUDA_VERSION}")
    set(eva_OUTPUT_NAME "${TARGET}-${version}-cuda${CUDA_VERSION}-${ARCHITECTURE}")
else() 
    set(eva_OUTPUT_NAME "${TARGET}-${version}-cpu-${ARCHITECTURE}")
endif()
message(STATUS ${eva_OUTPUT_NAME})

########################## 构建生产环境信息 ################################
# 获取操作系统版本信息
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(COMMAND lsb_release -d OUTPUT_VARIABLE OS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    execute_process(COMMAND sw_vers -productVersion OUTPUT_VARIABLE OS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    execute_process(COMMAND cmd.exe /C ver OUTPUT_VARIABLE OS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    # 去除残留的回车符和空白
    string(REGEX REPLACE "\r" "" OS_VERSION "${OS_VERSION}")  # 移除所有回车符
    string(STRIP "${OS_VERSION}" OS_VERSION)                  # 去除首尾空格
else()
    set(OS_VERSION "unknown OS")
endif()
# 获取处理器架构信息
set(PROCESSOR_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
# 拼接环境信息
string(CONCAT eva_ENVIRONMENT "${OS_VERSION} CPU: ${PROCESSOR_ARCHITECTURE}")
# 获取GPU架构信息
set(GPU_ARCH "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # 先尝试检测NVIDIA GPU
    find_program(NVIDIA_SMI nvidia-smi)
    if(NVIDIA_SMI)
        execute_process(COMMAND ${NVIDIA_SMI} --query-gpu=gpu_name --format=csv,noheader
            OUTPUT_VARIABLE NVIDIA_GPU_OUTPUT
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NVIDIA_GPU_OUTPUT)
            string(REGEX REPLACE "\n.*" "" FIRST_GPU_LINE "${NVIDIA_GPU_OUTPUT}")
            # 尝试解析架构名称
            if(FIRST_GPU_LINE MATCHES "V100")
                set(GPU_ARCH "NVIDIA Volta")
            elseif(FIRST_GPU_LINE MATCHES "A100")
                set(GPU_ARCH "NVIDIA Ampere")
            elseif(FIRST_GPU_LINE MATCHES "RTX 3")
                set(GPU_ARCH "NVIDIA Ampere")  # RTX 30系列属于Ampere
            elseif(FIRST_GPU_LINE MATCHES "RTX 2")
                set(GPU_ARCH "NVIDIA Turing")
            # 其他可能的匹配...
            else()
                set(GPU_ARCH "NVIDIA ${FIRST_GPU_LINE}")
            endif()
        endif()
    endif()

    # 如果没找到NVIDIA，尝试AMD
    if(NOT GPU_ARCH)
        find_program(ROCK_SMI rocm-smi)
        if(ROCK_SMI)
            execute_process(COMMAND ${ROCK_SMI} --showid
                OUTPUT_VARIABLE AMD_GPU_OUTPUT
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            # 解析AMD的输出，例如查找GPU型号
            if(AMD_GPU_OUTPUT MATCHES "GPU ID.*\n.*Device Name: (.*)\n")
                set(AMD_GPU_MODEL "${CMAKE_MATCH_1}")
                # 根据型号判断架构，如Radeon RX 7900 XT属于RDNA3
                if(AMD_GPU_MODEL MATCHES "Radeon RX 7")
                    set(GPU_ARCH "AMD RDNA3")
                else()
                    set(GPU_ARCH "AMD ${AMD_GPU_MODEL}")
                endif()
            endif()
        endif()
    endif()

    # 如果还没找到，尝试其他方式，如Intel或Apple Silicon
    if(NOT GPU_ARCH)
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            # macOS使用system_profiler
            execute_process(COMMAND system_profiler SPDisplaysDataType
                OUTPUT_VARIABLE MAC_GPU_OUTPUT
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(MAC_GPU_OUTPUT MATCHES "Chipset Model: (Apple [^\n]+)")
                set(GPU_ARCH "${CMAKE_MATCH_1}")
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            # 使用lspci查找Intel集成显卡
            execute_process(COMMAND lspci -nn | grep -i "VGA compatible controller"
                OUTPUT_VARIABLE PCI_GPU_OUTPUT
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(PCI_GPU_OUTPUT MATCHES "Intel Corporation (.*)\\[")
                set(GPU_ARCH "Intel ${CMAKE_MATCH_1}")
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            # 使用PowerShell获取显卡信息
            execute_process(COMMAND powershell -Command "Get-WmiObject Win32_VideoController | Select-Object -ExpandProperty Name"
                OUTPUT_VARIABLE WIN_GPU_OUTPUT
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            string(STRIP "${WIN_GPU_OUTPUT}" WIN_GPU_OUTPUT)
            if(WIN_GPU_OUTPUT MATCHES "NVIDIA")
                set(GPU_ARCH "NVIDIA ${WIN_GPU_OUTPUT}")
            elseif(WIN_GPU_OUTPUT MATCHES "AMD")
                set(GPU_ARCH "AMD ${WIN_GPU_OUTPUT}")
            elseif(WIN_GPU_OUTPUT MATCHES "Intel")
                set(GPU_ARCH "Intel ${WIN_GPU_OUTPUT}")
            endif()
        endif()
    endif()
endif()

if(GPU_ARCH)
    string(CONCAT eva_ENVIRONMENT "${eva_ENVIRONMENT}, GPU: ${GPU_ARCH}")
else()
    string(CONCAT eva_ENVIRONMENT "${eva_ENVIRONMENT}, GPU: unknown")
endif()
message(STATUS "生产环境: ${eva_ENVIRONMENT}")

# 定义传递给.cpp的变量
set(EVA_ENVIRONMENT ${eva_ENVIRONMENT}) # eva生产环境
set(EVA_VERSION ${eva_OUTPUT_NAME}) # eva型号
set(COMPILE_VERSION "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}") # 编译器版本
set(QT_VERSION_ ${Qt5Core_VERSION}) # qt版本
string(TIMESTAMP COMPILE_TIME "%Y-%m-%d %H:%M:%S")
set(EVA_PRODUCT_TIME ${COMPILE_TIME}) # eva编译时间
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmakeconfig.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmakeconfig.h
)

set(eva_OUTPUT_NAME ${eva_OUTPUT_NAME} PARENT_SCOPE) # 将子项目的变量传递给父项目
set(eva_ENVIRONMENT ${eva_ENVIRONMENT} PARENT_SCOPE) # 将子项目的变量传递给父项目
set(ARCHITECTURE ${ARCHITECTURE} PARENT_SCOPE) # 将子项目的变量传递给父项目