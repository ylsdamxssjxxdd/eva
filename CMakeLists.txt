################################# 若要更换第三方库版本，先阅读README_cmake.md ##############################

###################################### 基础配置 ######################################

cmake_minimum_required(VERSION 3.12) # 最低cmake版本
project(body) # 项目名
set(TARGET eva) # 最终生成的目标文件
set(CMAKE_INCLUDE_CURRENT_DIR ON) # 将项目目录也作为头文件包含目录
set(version b3667) # 打包版本
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release) # 设置最终eva的输出目录

###################################### 编译选项 ######################################
option(BODY_PACK                     "pack eva"                                   OFF) # 是否打包
option(GGML_CUDA                     "ggml: use CUDA"                             OFF) # 速度900%
option(GGML_VULKAN                   "ggml: use Vulkan"                           ON) # 速度250%,暂不支持sd

##################################### 处理编译选项 ####################################
option(BUILD_SHARED_LIBS        "build shared libraries"           ON) # 都用动态链接
option(SD_BUILD_SHARED_LIBS      "sd: build shared libs" ON)
# 这几个标志是互斥的
if(GGML_CUDA)
    add_compile_definitions(BODY_USE_CUDA) # 机体用cuda的标志
    add_compile_definitions(BODY_USE_GPU) # 机体用GPU的标志
    add_compile_definitions(GGML_USE_CUDA) # ggml用cuda的标志
    add_definitions(-DSD_USE_CUBLAS) # sd编译cuda版本的标志
elseif(GGML_VULKAN)
    find_package(Vulkan) # 不清楚为什么要放到这里才能找到，llama.cpp作为子项目自己找找不到
    add_compile_definitions(BODY_USE_VULKAN) # 机体用VULKAN的标志
    add_compile_definitions(BODY_USE_GPU) # 机体用GPU的标志
    add_definitions(-DSD_USE_VULKAN) # sd使用vulkan
else() 
    # 什么都没有默认编译cpu版本
endif()

if(BODY_PACK)
    if(WIN32)
        set(BODY_PACK_EXE WIN32) # 使用WIN32可以去掉控制台黑框
    elseif(UNIX)
        add_compile_definitions(BODY_LINUX_PACK) # 让第三方程序能找到appimage正确根路径
    endif()
endif()

# msvc设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8") # 支持代码中的中文
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /utf-8") # 支持代码中的中文
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8") # 支持代码中的中文
    if (GGML_CUDA) # 如果启用GGML_CUDA标志
        # 监视gpu部分
        list(APPEND extra_INCLUDES ui/utils/nvml.h) # 向extra_INCLUDES列表里添加文件
        list(APPEND extra_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/ui/utils/nvml.lib) # 向extra_LIBS列表里添加文件
        # 方便打包时找到cuda相关库
        find_package(CUDAToolkit)
        string(REGEX MATCH "^[0-9]+" CUDA_VERSION_MAJOR ${CUDAToolkit_VERSION})# 从CUDA版本字符串中提取主版本号
        string(REGEX MATCH "^[0-9]+\\.[0-9]+" CUDA_VERSION ${CUDAToolkit_VERSION})# 从CUDA版本字符串中提取主次版本号
        message(STATUS "cuda主版本 " ${CUDA_VERSION_MAJOR})
        message(STATUS "cuda库路径 " ${CUDAToolkit_BIN_DIR})
    endif()

# mingw设置编译选项
elseif(MINGW)
    set(LLAMA_WIN_VER "0x602" CACHE STRING "llama: Windows Version") # 原项目适配mingw
    add_compile_definitions(_WIN32_WINNT=${LLAMA_WIN_VER}) # 原项目适配mingw
    add_compile_definitions(_XOPEN_SOURCE=600) # 原项目适配mingw
    
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -std=c++11 -Wall -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads") # 编译优化
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s") # 链接优化，减少体积

# linux下编译选项
elseif(UNIX)
    message(STATUS "Compiling on Unix/Linux")
    option(LLAMA_STATIC             "llama: static link libraries"     OFF)

    if (GGML_CUDA) # 如果启用GGML_CUDA标志
        #list(APPEND extra_INCLUDES ui/utils/gpuchecker.h ui/utils/nvml.h) # 向extra_INCLUDES列表里添加文件
        list(APPEND extra_INCLUDES ui/utils/gpuchecker.h) # 向extra_INCLUDES列表里添加文件
        find_library(NVIDIA_ML_LIB nvidia-ml) # 寻找nvml.h的实现文件，so库文件
        list(APPEND extra_LIBS ${NVIDIA_ML_LIB}) # 向extra_LIBS列表里添加文件
        # 方便打包时找到cuda相关库
        find_package(CUDAToolkit)
        string(REGEX MATCH "^[0-9]+" CUDA_VERSION_MAJOR ${CUDAToolkit_VERSION})# 从CUDA版本字符串中提取主版本号
        string(REGEX MATCH "^[0-9]+\\.[0-9]+" CUDA_VERSION ${CUDAToolkit_VERSION})# 从CUDA版本字符串中提取主次版本号
        message(STATUS "cuda主版本 " ${CUDA_VERSION_MAJOR})
        message(STATUS "cuda库路径 " ${CUDAToolkit_BIN_DIR})
    endif()

endif()

##################################### 第三方程序相关 #####################################

add_subdirectory(ui/utils/audio/libsamplerate) # 添加libsamplerate音频重采样库
add_subdirectory(ui/utils/audio/libsndfile) # 添加libsndfile读取音频文件
list(APPEND extra_LIBS samplerate sndfile)

if(WIN32)
    set(sfx_NAME ".exe") # windows下生成的第三方程序带后缀
elseif(UNIX)
    set(sfx_NAME "") # linux下生成的第三方程序带后缀
endif()

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # sd原项目支持必须
add_definitions(-DGGML_MAX_NAME=128) # sd原项目支持必须
add_definitions(-DGGML_MAX_N_THREADS=512) # llama原项目支持必须

add_subdirectory(llama.cpp/ggml) # 添加ggml库

# whisper.cpp-5236f02
add_subdirectory(whisper.cpp) # 添加whisper.dll
add_subdirectory(whisper.cpp/examples) # 添加whisper执行程序

# stable-diffusion.cpp-e410aeb
add_subdirectory(stable-diffusion.cpp) # 添加stable-diffusion.dll
add_subdirectory(stable-diffusion.cpp/examples) # 添加sd执行程序

add_subdirectory(llama.cpp) # 添加llama库
set_target_properties(llama PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}) # 使llama.dll在build/Release目录下
if(UNIX AND NOT APPLE)
    set_target_properties(llama PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})# 针对 Linux 平台（生成 .so 文件）
endif()
# add_subdirectory(llama.cpp/examples/imatrix) # 生成重要性矩阵用
# add_subdirectory(llama.cpp/examples/main) # 测试用
# add_subdirectory(llama.cpp/examples/llava) # 测试用
# add_subdirectory(llama.cpp/examples/llama-bench) # 添加llama-bench
add_subdirectory(llama.cpp/examples/quantize) # 添加quantize
add_subdirectory(llama.cpp/examples/server) # 添加server

###################################### eva相关 ######################################
# 启用moc rcc uic编译器
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt相关库，qt库的bin目录需要在环境变量中
find_package(Qt5 COMPONENTS Widgets Network Script Multimedia TextToSpeech REQUIRED)
get_filename_component(Qt5_BIN_DIR "${Qt5_DIR}/../../../bin" ABSOLUTE) # 获取Qt5的bin目录

message(STATUS "Qt5的bin目录  ${Qt5_BIN_DIR}")
message(STATUS "build目录  ${CMAKE_BINARY_DIR}")
message(STATUS "eva输出目录  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}")

# 添加qt资源文件，因为启用了CMAKE_AUTORCC标志，所有resource_FILES添加到add_executable里就会自动生成.cpp文件
set(resource_FILES ui/resource/res.qrc ui/resource/ceval.qrc ui/resource/mmlu.qrc)

# 应用程序图标windows
set(logo_FILES ui/resource/logo/ico.rc)

# 创建eva.exe,添加源文件
add_executable(
${TARGET}
${BODY_PACK_EXE}
${logo_FILES} ${resource_FILES} ${extra_INCLUDES}
ui/main.cpp ui/utils.cpp ui/dialog.cpp ui/widget.cpp ui/expend.cpp ui/xbot.cpp ui/xnet.cpp ui/xtool.cpp
ui/widget.h ui/xbot.h ui/xtool.h ui/expend.h ui/xnet.h ui/xconfig.h
ui/widget.ui ui/expend.ui
ui/utils/gpuchecker.h ui/utils/waterwaveplaintextedit.h ui/utils/cpuchecker.h ui/utils/customqplaintextedit.h ui/utils/doubleqprogressbar.h ui/utils/cutscreendialog.h ui/utils/customtabwidget.h ui/utils/customswitchbutton.h
llama.cpp/examples/llava/clip.cpp llama.cpp/examples/llava/clip.h llama.cpp/examples/llava/llava.cpp llama.cpp/examples/llava/llava.h
)

if(GGML_VULKAN)
    set(eva_OUTPUT_NAME "${TARGET}-${version}-vulkan")
elseif(GGML_CUDA)
    message(STATUS "CUDA version: ${CUDA_VERSION}")
    set(eva_OUTPUT_NAME "${TARGET}-${version}-cuda${CUDA_VERSION}")
else() 
    set(eva_OUTPUT_NAME "${TARGET}-${version}-cpu")
endif()

# 链接相关库,生成可执行文件
target_link_libraries(${TARGET} PRIVATE common llama ${extra_LIBS} Qt5::Widgets Qt5::Network Qt5::Script Qt5::Multimedia Qt5::TextToSpeech)

add_dependencies(${TARGET} llama-server whisper-cli llama-quantize sd) # 确保eva最后生产，保证后处理顺利进行

# 后处理
if(MSVC)
    # 在生成目标之后执行 windeployqt，这个动作在eva生产之后
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${Qt5_BIN_DIR}/windeployqt.exe" "--release" "--no-translations" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${TARGET}${sfx_NAME}"
        COMMENT "custom Windeployqt ..."
    )

    # 复制cuda组件过来
    if(GGML_CUDA)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${CUDAToolkit_BIN_DIR}/cublas64_${CUDA_VERSION_MAJOR}.dll"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/cublas64_${CUDA_VERSION_MAJOR}.dll"
            COMMAND ${CMAKE_COMMAND} -E copy
                "${CUDAToolkit_BIN_DIR}/cudart64_${CUDA_VERSION_MAJOR}.dll"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/cudart64_${CUDA_VERSION_MAJOR}.dll"
            COMMAND ${CMAKE_COMMAND} -E copy
                "${CUDAToolkit_BIN_DIR}/cublasLt64_${CUDA_VERSION_MAJOR}.dll"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/cublasLt64_${CUDA_VERSION_MAJOR}.dll"
            COMMENT "Copying CUDA libraries to output directory"
            )
    endif()

    if(BODY_PACK)
        # 删除msvc编译器生成共享库产生的辅助文件以及一些非必须文件，这个动作在eva生产之后
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/vc_redist.x64.exe"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/EVA_TEMP"
            COMMENT "custom Removing ..."
        )

        # 使用winrar打包cuda版本的eva.exe为一个自解压程序，这个动作在eva生产之后
        add_custom_command(TARGET ${TARGET} POST_BUILD
            # -E chdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE} 切换到输出目录执行后面的命令
            # rar需要在环境变量中 -sfx是创建自解压程序 sfxconfig.txt是配置文件
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE} "rar" "a" "-sfx" "-z..\\..\\ui\\utils\\sfxconfig.txt" "${eva_OUTPUT_NAME}.exe" "./" "-r"
            COMMENT "custom Packing ..."
        )
    endif()
elseif(MINGW)
        get_filename_component(COMPILER_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY) # 获取编译器路径
        message(STATUS "Compiler bin directory: ${COMPILER_BIN_DIR}")
        # 在生成目标之后执行 windeployqt，这个动作在eva生产之后
        add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${Qt5_BIN_DIR}/windeployqt.exe" "--release" "--no-translations" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${TARGET}${sfx_NAME}"
        # 从编译器路径复制libgomp-1.dll到输出目录，ggml库依赖它
        COMMAND ${CMAKE_COMMAND} -E copy "${COMPILER_BIN_DIR}/libgomp-1.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/libgomp-1.dll"
        COMMENT "custom Windeployqt ..."
    )

    if(BODY_PACK)
        # 使用winrar打包为一个自解压程序，这个动作在eva生产之后
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/EVA_TEMP"
            # -E chdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE} 切换到输出目录执行后面的命令
            # rar需要在环境变量中 -sfx是创建自解压程序 sfxconfig.txt是配置文件
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE} "rar" "a" "-sfx" "-z..\\..\\ui\\utils\\sfxconfig.txt" "${eva_OUTPUT_NAME}${sfx_NAME}" "./" "-r"
            COMMENT "custom Packing ..."
        )
    endif()
elseif(UNIX)
# 使用linuxdeployqt打包所有组件为一个appimage
    if(BODY_PACK)

        # 自动打包操作，这个动作在eva生产之后
        add_custom_command(TARGET ${TARGET} POST_BUILD
            # 挪动文件
            COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/eva-*.AppImage"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/eva" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/AppDir/usr/bin/eva"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/llama-server" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/AppDir/usr/bin/llama-server"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/llama-quantize" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/AppDir/usr/bin/llama-quantize"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/sd" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/AppDir/usr/bin/sd"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/llama.cpp/ggml/src/libggml.so" "${CMAKE_BINARY_DIR}/Release/AppDir/usr/lib/libggml.so"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/libllama.so" "${CMAKE_BINARY_DIR}/Release/AppDir/usr/lib/libllama.so"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/whisper-cli" "${CMAKE_BINARY_DIR}/Release/AppDir/usr/bin/whisper-cli"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/bin/libstable-diffusion.so" "${CMAKE_BINARY_DIR}/Release/AppDir/usr/lib/libstable-diffusion.so"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/bin/libwhisper.so" "${CMAKE_BINARY_DIR}/Release/AppDir/usr/lib/libwhisper.so"
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/ui/utils ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/ui/utils/eva.desktop ${CMAKE_BINARY_DIR}/Release/AppDir/usr/share/applications/eva.desktop
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/ui/resource ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/ui/resource/logo/blue_logo.png ${CMAKE_BINARY_DIR}/Release/AppDir/usr/share/icons/hicolor/64x64/apps/blue_logo.png
            # 开始打包 
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/Release "linuxdeployqt" "${CMAKE_BINARY_DIR}/Release/AppDir/usr/share/applications/eva.desktop" "-appimage"
            # 重命名appimage文件，${CMAKE_SYSTEM_PROCESSOR}是系统架构
            COMMAND ${CMAKE_COMMAND} -E rename "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/eva-*.AppImage" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${eva_OUTPUT_NAME}-${CMAKE_SYSTEM_PROCESSOR}.AppImage"
            # 删除不需要的文件
            COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/eva" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/llama-server" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/llama-quantize" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/sd" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/libllama.so" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/whisper-cli" "${CMAKE_BINARY_DIR}/llama.cpp/ggml/src/libggml.so"
            COMMENT "custom linuxdeployqt ..."
        )
    endif()
endif()

# linuxdeployqt排除cuda相关库
# "-exclude-libs=libcublasLt.so.12,libcudart.so.12,libcublas.so.12,libcuda.so.1" 

