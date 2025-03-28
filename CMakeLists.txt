################################# 若要更换第三方库版本，先阅读README_cmake.md ##############################

###################################### 基础配置 ######################################

cmake_minimum_required(VERSION 3.12) # 最低cmake版本
project(body) # 项目名
set(TARGET eva) # 最终生成的目标文件
set(CMAKE_INCLUDE_CURRENT_DIR ON) # 将项目目录也作为头文件包含目录
set(version b4938) # 打包版本

# 设置所有构建类型下的可执行文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)

###################################### 编译选项 ######################################
option(BODY_PACK                     "pack eva"                                   OFF) # 是否打包
option(GGML_CUDA                     "ggml: use CUDA"                             OFF) # 速度900%
option(GGML_VULKAN                   "ggml: use Vulkan"                           OFF) # 速度250%
option(BODY_32BIT                    "support 32 BIT"                             OFF) # 速度-80% /(ㄒoㄒ)/~~

##################################### 处理编译选项 ####################################

option(BUILD_SHARED_LIBS        "build shared libraries"           ON) # 都用动态链接
option(SD_BUILD_SHARED_LIBS      "sd: build shared libs" ON) # 强制sd用动态链接
add_compile_definitions(_WIN32_WINNT=0x0601)
set(GGML_WIN_VER "0x601" CACHE STRING   "ggml: Windows version") # 控制ggml内部支持的windows版本为7,但是这样做再也不能支持mmap加速模型装载
# 这几个标志是互斥的
if(BODY_32BIT)
# 如果开启对32位 win7的支持，那么只能使用mingw编译器，并且关闭所有gpu cpu加速
    # 检查是否是 MinGW 编译器
    if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Windows"))
        message(FATAL_ERROR "This project requires MinGW.")
    endif()
    message(STATUS "32bit关闭所有加速")
    set(GGML_NATIVE OFF) # 不使用本机加速优化,提升兼容性
    option(GGML_FMA          "ggml: enable FMA"              OFF)
    option(GGML_F16C         "ggml: enable F16C"             OFF)
    option(GGML_AVX          "ggml: enable AVX"              OFF)
    option(GGML_AVX2         "ggml: enable AVX2"             OFF)
    option(GGML_BMI2         "ggml: enable BMI2"             OFF)
    option(GGML_F16C         "ggml: enable F16C"             OFF)
    option(GGML_CPU_AARCH64      "ggml: use runtime weight conversion of Q4_0 to Q4_X_X" OFF)

    add_compile_definitions(BODY_USE_32BIT) # 机体只支持32位的标志
elseif(GGML_CUDA)
    set(GGML_NATIVE OFF) # 不使用本机加速优化，会使cuda版本体积变大但是兼容性变强
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
        add_compile_definitions(BODY_WIN_PACK)
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
        list(APPEND extra_INCLUDES src/utils/nvml.h) # 向extra_INCLUDES列表里添加文件
        list(APPEND extra_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/nvml.lib) # 向extra_LIBS列表里添加文件
        # 方便打包时找到cuda相关库
        find_package(CUDAToolkit)
        string(REGEX MATCH "^[0-9]+" CUDA_VERSION_MAJOR ${CUDAToolkit_VERSION})# 从CUDA版本字符串中提取主版本号
        string(REGEX MATCH "^[0-9]+\\.[0-9]+" CUDA_VERSION ${CUDAToolkit_VERSION})# 从CUDA版本字符串中提取主次版本号
        message(STATUS "cuda主版本 " ${CUDA_VERSION_MAJOR})
        message(STATUS "cuda库路径 " ${CUDAToolkit_BIN_DIR})
    endif()
# mingw设置编译选项
elseif(MINGW)
    add_compile_definitions(_XOPEN_SOURCE=600) # 原项目适配mingw
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -Wall -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads") # 编译优化
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s") # 链接优化，减少体积
# linux下编译选项
elseif(UNIX)
    message(STATUS "Compiling on Unix/Linux")

    if (GGML_CUDA) # 如果启用GGML_CUDA标志
        #list(APPEND extra_INCLUDES src/utils/gpuchecker.h src/utils/nvml.h) # 向extra_INCLUDES列表里添加文件
        list(APPEND extra_INCLUDES src/utils/gpuchecker.h) # 向extra_INCLUDES列表里添加文件
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

add_subdirectory(thirdparty/libsamplerate) # 添加libsamplerate音频重采样库
add_subdirectory(thirdparty/libsndfile) # 添加libsndfile读取音频文件
list(APPEND extra_LIBS samplerate sndfile)

if(WIN32)
    set(sfx_NAME ".exe") # windows下生成的第三方程序带后缀
elseif(UNIX)
    set(sfx_NAME "") # linux下生成的第三方程序带后缀
endif()

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # sd原项目支持必须
add_definitions(-DGGML_MAX_NAME=128) # sd原项目支持必须
add_definitions(-DGGML_MAX_N_THREADS=512) # llama原项目支持必须

add_subdirectory(thirdparty/llama.cpp/ggml) # 添加ggml库

# whisper.cpp-34972db
add_subdirectory(thirdparty/whisper.cpp) # 添加whisper.dll
add_subdirectory(thirdparty/whisper.cpp/examples) # 添加whisper执行程序

# stable-diffusion.cpp-4570715
add_subdirectory(thirdparty/stable-diffusion.cpp) # 添加stable-diffusion.dll
add_subdirectory(thirdparty/stable-diffusion.cpp/examples) # 添加sd执行程序

add_subdirectory(thirdparty/llama.cpp) # 添加llama库
add_subdirectory(thirdparty/llama.cpp/common) # 添加common库
set_target_properties(llama PROPERTIES PUBLIC_HEADER thirdparty/llama.cpp/include/llama.h) # 包含llama.h头文件
if(UNIX AND NOT APPLE)
    set_target_properties(llama PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})# 针对 Linux 平台（生成 .so 文件）
endif()
# add_subdirectory(thirdparty/llama.cpp/examples/imatrix) # 生成重要性矩阵用
# add_subdirectory(thirdparty/llama.cpp/examples/main) # 测试用
# add_subdirectory(thirdparty/llama.cpp/examples/llava) # 测试用
add_subdirectory(thirdparty/llama.cpp/examples/llama-bench) # 添加llama-bench
add_subdirectory(thirdparty/llama.cpp/examples/tts) # 添加tts
add_subdirectory(thirdparty/llama.cpp/examples/quantize) # 添加quantize
add_subdirectory(thirdparty/llama.cpp/examples/server) # 添加server

###################################### eva相关 ######################################
# 启用moc rcc uic编译器
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt相关库，qt库的bin目录需要在环境变量中
find_package(Qt5 COMPONENTS Widgets Network Script Multimedia TextToSpeech REQUIRED)
get_filename_component(Qt5_BIN_DIR "${Qt5_DIR}/../../../bin" ABSOLUTE) # 获取Qt5的bin目录
add_subdirectory(src/utils) # 利用这里面的cmakelists.txt检查系统环境等
message(STATUS "Qt5的bin目录  ${Qt5_BIN_DIR}")
message(STATUS "build目录  ${CMAKE_BINARY_DIR}")
message(STATUS "eva输出目录  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# 添加qt资源文件，因为启用了CMAKE_AUTORCC标志，所有resource_FILES添加到add_executable里就会自动生成.cpp文件
if(WIN32)
    set(resource_FILES resource/res.qrc resource/ceval.qrc resource/mmlu.qrc)
else() # linux下打包字体
    set(resource_FILES resource/res.qrc resource/ceval.qrc resource/mmlu.qrc resource/font.qrc)
endif()

# 应用程序图标windows
set(logo_FILES resource/logo/ico.rc)

# 创建eva.exe,添加源文件
add_executable(
${TARGET}
${BODY_PACK_EXE}
${logo_FILES} ${resource_FILES} ${extra_INCLUDES}
src/main.cpp src/widget.cpp src/widget_funcs.cpp src/expend.cpp src/xbot.cpp src/xnet.cpp src/xtool.cpp
src/widget.h src/xbot.h src/xtool.h src/expend.h src/xnet.h src/xconfig.h
src/widget.ui src/expend.ui src/date_dialog.ui src/settings_dialog.ui
src/utils/gpuchecker.h src/utils/waterwaveplaintextedit.h src/utils/cpuchecker.h src/utils/customqplaintextedit.h src/utils/doubleqprogressbar.h src/utils/cutscreendialog.h src/utils/customtabwidget.h src/utils/customswitchbutton.h
thirdparty/llama.cpp/examples/llava/clip.cpp thirdparty/llama.cpp/examples/llava/clip.h thirdparty/llama.cpp/examples/llava/llava.cpp thirdparty/llama.cpp/examples/llava/llava.h
)

# 链接相关库,生成可执行文件
target_link_libraries(${TARGET} PRIVATE common llama ${extra_LIBS} Qt5::Widgets Qt5::Network Qt5::Script Qt5::Multimedia Qt5::TextToSpeech)
add_dependencies(${TARGET} llama-server whisper-cli llama-quantize llama-tts llama-bench sd) # 确保eva最后生产，保证后处理顺利进行
message(STATUS "生产环境: ${eva_ENVIRONMENT}")
message(STATUS "eva型号：${eva_OUTPUT_NAME}")

if(ARCHITECTURE MATCHES "aarch64|arm")
    message(STATUS "arm下关闭所有cpu加速")
    set(GGML_NATIVE OFF) # 不使用本机加速优化,提升兼容性
    option(GGML_FMA          "ggml: enable FMA"              OFF)
    option(GGML_F16C         "ggml: enable F16C"             OFF)
    option(GGML_AVX          "ggml: enable AVX"              OFF)
    option(GGML_AVX2         "ggml: enable AVX2"             OFF)
    option(GGML_BMI2         "ggml: enable BMI2"             OFF)
    option(GGML_F16C         "ggml: enable F16C"             OFF)
    option(GGML_CPU_AARCH64      "ggml: use runtime weight conversion of Q4_0 to Q4_X_X" OFF)
endif()

# 后处理
if(MSVC)
    # 在生成目标之后执行 windeployqt，这个动作在eva生产之后
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${Qt5_BIN_DIR}/windeployqt.exe" "--release" "--no-translations" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}${sfx_NAME}"
        COMMENT "custom Windeployqt ..."
    )
    # 复制转换脚本到scripts目录下
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/gguf-py" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scripts/gguf-py"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/convert_hf_to_gguf.py" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scripts/convert_hf_to_gguf.py"
        COMMENT "copy scripts"
    )

    # 复制cuda组件过来
    if(GGML_CUDA)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${CUDAToolkit_BIN_DIR}/cublas64_${CUDA_VERSION_MAJOR}.dll"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cublas64_${CUDA_VERSION_MAJOR}.dll"
            COMMAND ${CMAKE_COMMAND} -E copy
                "${CUDAToolkit_BIN_DIR}/cudart64_${CUDA_VERSION_MAJOR}.dll"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cudart64_${CUDA_VERSION_MAJOR}.dll"
            COMMAND ${CMAKE_COMMAND} -E copy
                "${CUDAToolkit_BIN_DIR}/cublasLt64_${CUDA_VERSION_MAJOR}.dll"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cublasLt64_${CUDA_VERSION_MAJOR}.dll"
            COMMENT "Copying CUDA libraries to output directory"
            )
    endif()

    if(BODY_PACK)
        # 删除msvc编译器生成共享库产生的辅助文件以及一些非必须文件，这个动作在eva生产之后
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vc_redist.x64.exe"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/EVA_TEMP"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}${sfx_NAME}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${eva_OUTPUT_NAME}${sfx_NAME}" # 复制并重命名
            COMMENT "custom Removing ..."
            # rar需要在环境变量中 -sfx是创建自解压程序 sfxconfig.txt是配置文件
            # COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} "rar" "a" "-sfx" "-z..\\..\\ui\\utils\\sfxconfig.txt" "${eva_OUTPUT_NAME}.exe" "./" "-r"
            # COMMENT "custom Packing ..."
        )

    endif()

elseif(MINGW)
    get_filename_component(COMPILER_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY) # 获取编译器路径
    message(STATUS "Compiler bin directory: ${COMPILER_BIN_DIR}")
    # 在生成目标之后执行 windeployqt，这个动作在eva生产之后
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${Qt5_BIN_DIR}/windeployqt.exe" "--release" "--no-translations" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}${sfx_NAME}"
        # 从编译器路径复制libgomp-1.dll到输出目录，ggml库依赖它
        COMMAND ${CMAKE_COMMAND} -E copy "${COMPILER_BIN_DIR}/libgomp-1.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libgomp-1.dll"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}${sfx_NAME}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${eva_OUTPUT_NAME}${sfx_NAME}" # 复制并重命名
        COMMENT "custom Windeployqt ..."
    )
    # 复制转换脚本到scripts目录下
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/gguf-py" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scripts/gguf-py"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/convert_hf_to_gguf.py" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scripts/convert_hf_to_gguf.py"
        COMMENT "copy scripts"
    )

elseif(UNIX)
# 使用linuxdeployqt打包所有组件为一个appimage
    # 复制转换脚本到scripts目录下
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/gguf-py" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scripts/gguf-py"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/convert_hf_to_gguf.py" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scripts/convert_hf_to_gguf.py"
        COMMENT "copy scripts"
    )

    if(BODY_PACK)

        # 构建Appdir目录
        add_custom_command(TARGET ${TARGET} POST_BUILD
            # 挪动文件
            COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/eva-*.AppImage"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/eva" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/bin/eva"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/llama-bench" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/bin/llama-bench"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/llama-server" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/bin/llama-server"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/llama-quantize" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/bin/llama-quantize"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/llama-tts" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/bin/llama-tts"
            # COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/thirdparty/llama.cpp/ggml/src/libggml.so" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libggml.so"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libllama.so" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libllama.so"
            
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/bin/libstable-diffusion.so" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libstable-diffusion.so"
            # COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/bin/libsd-ggml.so" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libsd-ggml.so"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sd" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/bin/sd"

            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/thirdparty/whisper.cpp/src/libwhisper.so.1" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libwhisper.so.1" # 这个是快捷方式
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/thirdparty/whisper.cpp/src/libwhisper.so.1.7.2" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libwhisper.so.1.7.2" # 注意版本号
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/whisper-cli" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/bin/whisper-cli"

            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libsd-ggml.so" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libsd-ggml.so"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libsd-ggml-cpu.so" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libsd-ggml-cpu.so"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libsd-ggml-base.so" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libsd-ggml-base.so"
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/src/utils ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/eva.desktop ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/share/applications/eva.desktop
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/resource ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/resource/logo/blue_logo.png ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/share/icons/hicolor/64x64/apps/blue_logo.png
        )

        # 复制转换脚本到scripts目录下
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/gguf-py" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/scripts/gguf-py"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/convert_hf_to_gguf.py" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/scripts/convert_hf_to_gguf.py"
            COMMENT "copy scripts"
        )

        # 执行打包 使用linuxdeploy linuxdeploy-plugin-qt appimagetool打包  生成的.appimage文件在构建目录下
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND "${Qt5_BIN_DIR}/linuxdeploy" "--appdir" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir"
            COMMAND env QMAKE="${Qt5_BIN_DIR}/qmake" "${Qt5_BIN_DIR}/linuxdeploy-plugin-qt" "--appdir" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir"
            COMMAND "${Qt5_BIN_DIR}/appimagetool" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir" "--runtime-file" "${Qt5_BIN_DIR}/runtime-appimage" "${eva_OUTPUT_NAME}.AppImage"
        
            )

        # 有条件打包其它组件
        if(GGML_CUDA)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libsd-ggml-cuda.so\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libsd-ggml-cuda.so\"
            )
        elseif(GGML_VULKAN)   
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libsd-ggml-vulkan.so\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppDir/usr/lib/libsd-ggml-vulkan.so\"
            )
        endif()
    endif()
endif()

