################################# 先阅读README_cmake.md ##############################
###################################### 基础配置 ######################################
cmake_minimum_required(VERSION 3.12) # 最低cmake版本
project(body) # 项目名
set(TARGET eva) # 最终生成的目标文件名
set(CMAKE_INCLUDE_CURRENT_DIR ON) # 将项目目录也作为头文件包含目录
set(version b3216) # 打包版本

###################################### 编译选项 ######################################
option(BODY_PACK                    "pack eva"                                   OFF) # 是否打包
option(LLAMA_CUDA                   "llama: use CUDA"                            ON) # 速度900%
option(LLAMA_VULKAN                 "llama: use Vulkan"                          OFF) # 速度250%,暂不支持sd加速
option(BODY_32BIT                   "build 32 bit"                               OFF) # 编译32位的选项！不能装载超过1G的模型
##################################### 处理编译选项 ####################################
# 这几个标志是互斥的
if(BODY_32BIT)
    add_compile_definitions(BODY_USE_32BIT) # 编译32位的标志
elseif(LLAMA_CUDA)
    add_compile_definitions(BODY_USE_CUDA) # 编译cuda的标志
    add_compile_definitions(BODY_USE_GPU) # 使用GPU的标志
    add_compile_definitions(GGML_USE_CUDA) # whisper用编译cuda的标志
    option(LLAMA_CUDA_FORCE_MMQ                  "llama: always use mmq kernels instead of cuBLAS"  OFF) # 原项目mmq加速支持
    option(LLAMA_CUDA_FORCE_CUBLAS               "llama: always use cuBLAS instead of mmq kernels"  OFF) # 原项目cublas加速支持
    # add_compile_definitions(BODY_USE_SPEECH)#由于win7下不支持，只在使用cuda时开启语音朗读
elseif(LLAMA_VULKAN)
    find_package(Vulkan) # 不清楚为什么要放到这里才能找到，llama.cpp作为子项目自己找找不到
    add_compile_definitions(BODY_USE_VULKAN) # 编译VULKAN的标志
    add_compile_definitions(BODY_USE_GPU) # 使用GPU的标志
else() 
    # 什么都没有默认编译64bit版本
endif()

if(BODY_PACK)
    set(BODY_PACK_EXE WIN32) # 使用WIN32可以去掉控制台黑框
endif()

# msvc设置编译选项
if(MSVC)
    option(BUILD_SHARED_LIBS        "build shared libraries"           ON) # 如果用msvc编译器就动态链接
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8") # 支持代码中的中文
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /utf-8") # 支持代码中的中文
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8") # 支持代码中的中文
    if (LLAMA_CUDA) # 如果启用LLAMA_CUDA标志
        # 监视gpu部分
        list(APPEND extra_INCLUDES utils/gpuchecker.h utils/nvml.h) # 向extra_INCLUDES列表里添加文件
        list(APPEND extra_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/utils/nvml.lib) # 向extra_LIBS列表里添加文件
        list(APPEND sd_EXTRA_COMMAND "-DSD_CUBLAS=ON") # 启用sd的cuda编译标志
    endif()

# mingw设置编译选项
elseif(MINGW)
    option(BUILD_SHARED_LIBS        "build shared libraries"           OFF)
    set(LLAMA_WIN_VER "0x602" CACHE STRING "llama: Windows Version") # 原项目适配mingw
    add_compile_definitions(_WIN32_WINNT=${LLAMA_WIN_VER}) # 原项目适配mingw
    add_compile_definitions(_XOPEN_SOURCE=600) # 原项目适配mingw
    
    set(CMAKE_CXX_FLAGS_RELEASE "-static") # 使用静态编译
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -std=c++11 -Wall -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads") # 编译优化
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s") # 链接优化，减少体积
    
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a") # 这里是强制静态链接libgomp.a以支持openmp算法
    list(APPEND sd_EXTRA_COMMAND "-G" "\"MinGW Makefiles\"") # 对sd的自定义编译强制使用mingw编译器
# linux下编译选项
elseif(UNIX)
    message(STATUS "Compiling on Unix/Linux")
    option(BUILD_SHARED_LIBS        "build shared libraries"           ON)
    option(LLAMA_STATIC             "llama: static link libraries"     OFF)

    if (LLAMA_CUDA) # 如果启用LLAMA_CUDA标志
        list(APPEND extra_INCLUDES utils/gpuchecker.h utils/nvml.h) # 向extra_INCLUDES列表里添加文件
        find_library(NVIDIA_ML_LIB nvidia-ml) # 寻找nvml.h的实现文件，so库文件
        list(APPEND extra_LIBS ${NVIDIA_ML_LIB}) # 向extra_LIBS列表里添加文件
        list(APPEND sd_EXTRA_COMMAND "-DSD_CUBLAS=ON") # 启用sd的cuda编译标志
    endif()

endif()

##################################### 第三方程序相关 #####################################
if(WIN32)
    set(sfx_NAME ".exe") # windows下生成的第三方程序带后缀
elseif(UNIX)
    set(sfx_NAME "") # linux下生成的第三方程序带后缀
endif()
# 添加llama相关项目,将会强制应用父项目的编译设置
add_subdirectory(llama.cpp) # 添加llama库, 包含了更基础的ggml库
set_target_properties(llama PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}) # 使llama.dll在build/Release目录下
add_subdirectory(llama.cpp/examples/imatrix) # 生成重要性矩阵用
add_subdirectory(llama.cpp/examples/main) # 测试用

# 先编译好server.exe等并放到utils目录下，等待eva打包进资源文件
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/utils") # 设置新的输出目录

add_subdirectory(llama.cpp/examples/quantize) # 添加quantize.exe
add_subdirectory(llama.cpp/examples/server) # 添加server.exe

set(TARGET_whisper whisper) # 添加whisper.exe
add_executable(${TARGET_whisper} 
whisper.cpp/main.cpp whisper.cpp/whisper.cpp whisper.cpp/common.cpp
whisper.cpp/whisper.h whisper.cpp/common.h whisper.cpp/dr_wav.h
llama.cpp/ggml.h llama.cpp/ggml-quants.h)
target_link_libraries(${TARGET_whisper} PRIVATE llama) # 这里直接将whisper链接llama.cpp的llama库
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release) # 设置最终eva.exe的输出目录

# 构建sd.exe并放到utils目录下
set(stable_DIFFUSION_DIR "${PROJECT_SOURCE_DIR}/stable-diffusion.cpp")
add_custom_command(TARGET ${TARGET_whisper} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E chdir ${stable_DIFFUSION_DIR} ${CMAKE_COMMAND} -E make_directory build
    COMMAND ${CMAKE_COMMAND} -E chdir ${stable_DIFFUSION_DIR}/build "cmake" ".." ${sd_EXTRA_COMMAND} "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${stable_DIFFUSION_DIR}/build/bin/Release"
    COMMAND ${CMAKE_COMMAND} -E chdir ${stable_DIFFUSION_DIR}/build "cmake" "--build" "." "--config Release"
    COMMAND ${CMAKE_COMMAND} -E chdir ${stable_DIFFUSION_DIR}/build ${CMAKE_COMMAND} -E copy ${stable_DIFFUSION_DIR}/build/bin/Release/sd${sfx_NAME} ${PROJECT_SOURCE_DIR}/utils/sd${sfx_NAME}
    COMMENT "custom Cmake ..."
)

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
# 注意32位不能超过10MB所有分了很多个，不用qt5_add_big_resources是因为它会先于exe文件生成前执行
if(WIN32)
    set(resource_FILES utils/logo.qrc utils/server_exe.qrc utils/quantize_exe.qrc utils/whisper_exe.qrc utils/sd_exe.qrc utils/ceval.qrc utils/mmlu.qrc)
elseif(UNIX)
    set(resource_FILES utils/logo.qrc utils/server.qrc utils/quantize.qrc utils/whisper.qrc utils/sd.qrc utils/ceval.qrc utils/mmlu.qrc)
endif()
# 应用程序图标windows
set(logo_FILES utils/ui/ico.rc)

# 创建eva.exe,添加源文件
add_executable(
${TARGET}
${BODY_PACK_EXE}
${logo_FILES} ${resource_FILES} ${extra_INCLUDES}
main.cpp utils.cpp dialog.cpp widget.cpp expend.cpp xbot.cpp xnet.cpp xtool.cpp
widget.h xbot.h xtool.h expend.h xnet.h xconfig.h
widget.ui expend.ui
utils/cpuchecker.h utils/customqplaintextedit.h utils/doubleqprogressbar.h utils/cutscreendialog.h utils/customtabwidget.h utils/customswitchbutton.h
llama.cpp/examples/llava/clip.cpp llama.cpp/examples/llava/clip.h llama.cpp/examples/llava/llava.cpp llama.cpp/examples/llava/llava.h
)

if(BODY_32BIT)
    set(eva_OUTPUT_NAME "${TARGET}-${version}-32bit")
elseif(LLAMA_VULKAN)
    set(eva_OUTPUT_NAME "${TARGET}-${version}-vulkan")
elseif(LLAMA_CUDA)
    set(eva_OUTPUT_NAME "${TARGET}-${version}-cuda")
else() 
    set(eva_OUTPUT_NAME "${TARGET}-${version}-64bit")
endif()

if(MINGW)
    set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "${eva_OUTPUT_NAME}") # 设置输出文件的文件名 MSVC编译器下输出的未打包文件名是eva.exe
endif()

# 确保第三方程序先于eva编译
add_dependencies(${TARGET} llama-server whisper llama-quantize) # 原先的server等添加了llama-前缀

# 链接相关库,生成可执行文件
target_link_libraries(${TARGET} PRIVATE common llama ${extra_LIBS} Qt5::Widgets Qt5::Network Qt5::Script Qt5::Multimedia Qt5::TextToSpeech)

# 后处理
if(MSVC)
    # 在生成目标之后执行 windeployqt
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${Qt5_BIN_DIR}/windeployqt.exe" "--release" "--no-translations" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${TARGET}.exe"
        COMMENT "custom Windeployqt ..."
    )
    # 删除msvc编译器生成共享库产生的辅助文件以及一些非必须文件
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${eva_OUTPUT_NAME}.exp" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${eva_OUTPUT_NAME}.lib"
        COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/vc_redist.x64.exe"
        COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${eva_OUTPUT_NAME}.exe"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/EVA_TEMP"
        COMMENT "custom Removing ..."
    )
    if(BODY_PACK)
        # 使用winrar打包cuda版本的eva.exe为一个自解压程序
        add_custom_command(TARGET ${TARGET} POST_BUILD
            # -E chdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE} 切换到输出目录执行后面的命令
            # rar需要在环境变量中 -sfx是创建自解压程序 sfxconfig.txt是配置文件
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE} "rar" "a" "-sfx" "-z..\\..\\utils\\sfxconfig.txt" "${eva_OUTPUT_NAME}.exe" "./" "-r"
            COMMENT "custom Packing ..."
        )
    endif()
endif()

