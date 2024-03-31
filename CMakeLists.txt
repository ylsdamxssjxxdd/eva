######################################手动修改######################################
#llama.cpp-b2554
#llama.cpp项目手动更新后注意cmakelists.txt中的add_compile_definitions选项
#修改ggml.c中的FILE * ggml_fopen(const char * fname, const char * mode)函数
#只要保留return fopen(fname, mode);其余删除
#----------------------------------------------------------------------------------
#stable-diffusion.cpp master-48bcce4
#stable-diffusion.cpp/cmakelists.txt中添加
# mingw设置编译选项
# if(MINGW)
    # set(CMAKE_CXX_FLAGS_RELEASE "-static")#对齐静态编译的标志
    # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -std=c++11 -march=native -Wall -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads")    
    # set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s") #编译优化
# endif()
#将stable-diffusion.cpp链接的ggml库全部更名为ggml-sd(5处)避免冲突，直接和老文件对比吧
#stable-diffusion.cpp/cmakelists.txt/target_link_libraries(${SD_LIB} PUBLIC ggml-sd zip)以及stable-diffusion.cpp/ggml/src/cmakelists.txt
#----------------------------------------------------------------------------------
#whisper.cpp master-298c74
#
#----------------------------------------------------------------------------------
#camke
#注意cmake构建类型应该设置为 Release
#----------------------------------------------------------------------------------

######################################基础配置######################################
cmake_minimum_required(VERSION 3.12)
project(body)
set(TARGET eva)
set(CMAKE_INCLUDE_CURRENT_DIR ON)#将项目目录也作为头文件包含目录
######################################编译选项######################################
option(LLAMA_CUBLAS                          "llama: use CUDA"                                  OFF)
option(LLAMA_CLBLAST                         "llama: use CLBlast"                               OFF)
option(BODY_32BIT                            "build 32 bit"                                     OFF)#编译32位的选项！
#####################################处理编译选项####################################
#这三个标志是互斥的
if(BODY_32BIT)
    add_compile_definitions(BODY_USE_32BIT)#编译32位的标志
elseif(LLAMA_CUBLAS)
    add_compile_definitions(BODY_USE_CUBLAST)#编译cuda的标志
    add_compile_definitions(GGML_USE_CUBLAS)#由于whisper和llama.cpp暂时有冲突不启用
    set(GGML_CUBLAS ON)#sd用cuda
    add_definitions(-DSD_USE_CUBLAS)#sd用cuda
elseif(LLAMA_CLBLAST)
    add_compile_definitions(BODY_USE_CLBLAST)#编译clblast的标志
#sd的cpu加速暂不支持mingw编译器
# else()
#     add_definitions(-DSD_USE_FLASH_ATTENTION)#sd用cpu加速
endif()
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)#原项目 编译选项        
# msvc设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8") # 支持代码中的中文
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /utf-8") # 支持代码中的中文
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8") # 支持代码中的中文
    
    if (LLAMA_CUBLAS)# 如果启用LLAMA_CUBLAS标志
        # 监视gpu部分
        set(EXTRA_FILES
            utils/gpuchecker.h
            utils/gpuchecker.cpp
            utils/nvml.h
            )
        set(EXTRA_LIBS
            ${CMAKE_CURRENT_SOURCE_DIR}/utils/nvml.lib #注意库文件最好用这样的绝对路径 
        )
    endif()
# mingw设置编译选项
elseif(MINGW)
    set(LLAMA_WIN_VER "0x602" CACHE STRING "llama: Windows Version")#原项目 编译选项
    add_compile_definitions(_WIN32_WINNT=${LLAMA_WIN_VER})#原项目 编译选项
    add_compile_definitions(_XOPEN_SOURCE=600)#原项目 编译选项

    set(CMAKE_CXX_FLAGS_RELEASE "-static")#对齐静态编译的标志
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -std=c++11 -Wall -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads")    
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s") #编译优化
endif()
#####################################llama.cpp相关#####################################
#添加llama相关项目,将会强制应用父项目的编译设置
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(llama.cpp)# 添加子项目主要库
add_subdirectory(llama.cpp/examples/imatrix)# 生成重要性矩阵用
add_subdirectory(llama.cpp/examples/llava)#视觉和server必须
add_subdirectory(stable-diffusion.cpp)# 添加stable-diffusion.cpp基础库，为避免冲突已将ggml库更名为ggml-sd
#先编译好server并放到utils目录下，等待eva打包进资源文件
set(OLD_CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})# 保存当前的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/utils")# 设置新的输出目录

add_subdirectory(llama.cpp/examples/quantize)# 模型量化用
add_subdirectory(llama.cpp/examples/server)# 添加子目录
add_subdirectory(stable-diffusion.cpp/examples/cli)#添加stable-diffusion.cpp运行程序

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OLD_CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})# 恢复之前的输出目录

#####################################whisper.cpp相关#####################################
#先编译好server并放到utils目录下，等待eva打包进资源文件
set(OLD_CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})# 保存当前的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/utils")# 设置新的输出目录

set(TARGET_whisper whisper)
add_executable(${TARGET_whisper} 
whisper.cpp/main.cpp
whisper.cpp/whisper.h
whisper.cpp/whisper.cpp
whisper.cpp/common.h
whisper.cpp/common.cpp
whisper.cpp/dr_wav.h
llama.cpp/ggml.h
llama.cpp/ggml-mpi.h
llama.cpp/ggml-quants.h
)

target_link_libraries(${TARGET_whisper} PRIVATE ggml ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(${TARGET_whisper} PRIVATE cxx_std_11)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OLD_CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})# 恢复之前的输出目录

# 第三方exe程序添加到资源文件
set(THRIDEXE_FILE utils/thrid_exe.qrc)
######################################eva相关######################################
# 启用moc rcc uic编译器
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
# 查找Qt相关库
find_package(Qt5 COMPONENTS Widgets Network Script Multimedia TextToSpeech REQUIRED)

# 资源文件
set(RESOURCE_FILES
utils/logo.qrc
${THRIDEXE_FILE}
utils/ceval.qrc
)

# 应用程序图标windows
set(LOGO_FILES
utils/ui/ico.rc
)

# 设置可执行文件,添加源文件,使用WIN32可以去掉控制台黑框
add_executable(${TARGET}
#WIN32
${LOGO_FILES}
${RESOURCE_FILES}
${EXTRA_FILES}
main.cpp
utils.cpp
dialog.cpp
widget.cpp
expend.h
expend.cpp
widget.h
xbot.cpp
xbot.h
xnet.cpp
xnet.h
xtool.cpp
xtool.h
xconfig.h
widget.ui
expend.ui

utils/doubleqprogressbar.cpp
utils/doubleqprogressbar.h
utils/CutScreenDialog.h
utils/CutScreenDialog.cpp
utils/CustomTabWidget.h
utils/CustomTabBar.h
)

#确保第三方程序先于eva编译
add_dependencies(${TARGET} server whisper sd)
# 链接相关库,生成可执行文件
target_link_libraries(${TARGET} PRIVATE common llava llama ${EXTRA_LIBS} ${CMAKE_THREAD_LIBS_INIT} Qt5::Widgets Qt5::Network Qt5::Script Qt5::Multimedia Qt5::TextToSpeech)
target_compile_features(${TARGET} PRIVATE cxx_std_11)

