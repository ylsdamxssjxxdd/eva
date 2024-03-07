######################################基础配置######################################
cmake_minimum_required(VERSION 3.12)
project(body)
set(TARGET eva)
set(CMAKE_INCLUDE_CURRENT_DIR ON)#将项目目录也作为头文件包含目录
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (default to Release)" FORCE)# 强制设置构建类型为 Release
#更新子模块 git submodule update --remote
######################################编译选项######################################
option(LLAMA_CUBLAS                          "llama: use CUDA"                                  ON)
option(LLAMA_CLBLAST                         "llama: use CLBlast"                               OFF)
option(BODY_32BIT                            "build 32 bit"                                     OFF)#编译32位的标志
#####################################处理编译选项####################################
#这三个标志是互斥的
if(BODY_32BIT)
    add_compile_definitions(BODY_USE_32BIT)#告诉源码编译32位标志
elseif(LLAMA_CUBLAS)
    add_compile_definitions(BODY_USE_CUBLAST)#编译body-cuda标志
elseif(LLAMA_CLBLAST)
    add_compile_definitions(BODY_USE_CLBLAST)#编译body-cuda标志
endif()           
# msvc设置编译选项
if(MSVC)
    if (LLAMA_CUBLAS)# 如果启用LLAMA_CUBLAS标志
        set(EXTRA_FILES
            utils/gpuchecker.h
            utils/gpuchecker.cpp
            utils/nvml.h
            )
        #注意库文件最好用这样的绝对路径    
        set(EXTRA_LIBS
            ${CMAKE_CURRENT_SOURCE_DIR}/utils/nvml.lib
        )
    endif()
# mingw设置编译选项
elseif(MINGW)
    set(CMAKE_CXX_FLAGS_RELEASE "-static")#对齐静态编译的标志
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -std=c++11 -Wall -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads")    
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s") #编译优化
endif()
#####################################llama.cpp相关#####################################
#添加llama相关项目,将会强制应用父项目的编译设置
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(llama.cpp)
add_subdirectory(llama.cpp/examples/main)
add_subdirectory(llama.cpp/examples/llava)
#先编译好server并放到utils目录下，等待eva打包进资源文件
set(OLD_CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})# 保存当前的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/utils")# 设置新的输出目录

add_subdirectory(llama.cpp/examples/server)# 添加子目录
set(SERVER_FILE utils/server.qrc)# 添加到资源文件

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OLD_CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})# 恢复之前的输出目录
######################################eva相关######################################
# 启用moc rcc uic编译器
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
# 查找Qt相关库
find_package(Qt5 COMPONENTS Widgets Network Script REQUIRED)

# 资源文件
set(RESOURCE_FILES
utils/logo.qrc
${SERVER_FILE}
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
xbert.cpp
xbert.h
xtool.cpp
xtool.h
xconfig.h
widget.ui
expend.ui
utils/customplaintextedit.cpp
utils/customplaintextedit.h
utils/doubleqprogressbar.cpp
utils/doubleqprogressbar.h
utils/CutScreenDialog.h
utils/CutScreenDialog.cpp
)

#确保server先于eva编译
add_dependencies(${TARGET} server)
# 链接相关库,生成可执行文件
target_link_libraries(${TARGET} PRIVATE common llava llama ${EXTRA_LIBS} ${CMAKE_THREAD_LIBS_INIT} Qt5::Widgets Qt5::Network Qt5::Script)
target_compile_features(${TARGET} PRIVATE cxx_std_11)

