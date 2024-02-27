# 1.截获装载进度,需要修改llama.cpp里的llama_load_model_from_file函数
# int cur_percent = *cur_percentage_p;
# while (percentage > cur_percent) 
# {
#     cur_percent++;
#     LLAMA_LOG_INFO("load_percent = %d",cur_percent);
#     if (cur_percent >= 100) {
#         LLAMA_LOG_INFO("\n");
#     }
# }
# if(percentage > *cur_percentage_p)
# {
#     *cur_percentage_p = percentage;
# }
#
#
# 2.关掉server的日志
# option(LLAMA_SERVER_VERBOSE "Build verbose logging option for Server" OFF)
#
# 3.编译出server后手动更新到资源文件中
#
######################################基础配置######################################
cmake_minimum_required(VERSION 3.27)
project(body)
set(TARGET eva)
set(CMAKE_INCLUDE_CURRENT_DIR ON)#将项目目录也作为头文件包含目录
######################################编译选项######################################
# 启用moc rcc uic编译器
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

#原项目编译选项
option(LLAMA_CUBLAS                          "llama: use CUDA"                                  OFF)
option(LLAMA_CLBLAST                         "llama: use CLBlast"                               OFF)
option(BODY_32BIT                           "build 32 bit"                 OFF)#编译32位的cmake标志

# msvc设置编译选项
if(MSVC)
    if (LLAMA_CUBLAS)# 如果启用LLAMA_CUBLAS标志
        set(SERVER_FILE utils/server_cuda.qrc)
        add_compile_definitions(BODY_USE_CUBLAST)#编译body-cuda标志
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
    #set(CMAKE_CXX_FLAGS_RELEASE "/MT")#对齐静态编译的标志
    set(Qt5_DIR D:/Qt/5.15/qt5.15.10-msvc/lib/cmake/Qt5) #动态链接库
# mingw设置编译选项   
elseif(MINGW)
    #set(Qt5_DIR D:/Qt/5.15/qt5.15.10-mingw81_32-static/lib/cmake/Qt5) #静态链接库
    set(Qt5_DIR D:/Qt/5.15/qt5.15.10-mingw81_64-static/lib/cmake/Qt5) #静态链接库   
    if(BODY_32BIT)
        add_compile_definitions(BODY_USE_32BIT)#编译32位的编译器标志
    endif()    
    
    if (LLAMA_CLBLAST)
        set(SERVER_FILE utils/server_clblast.qrc)
    elseif(BODY_32BIT)
        set(SERVER_FILE utils/server_32bit.qrc)    
    else()
        set(SERVER_FILE utils/server.qrc)
    endif()

    set(CMAKE_CXX_FLAGS_RELEASE "-static")#对齐静态编译的标志
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-keep-inline-dllexport -O2 -std=gnu++11 -Wall -Wextra -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s") #编译优化
        
endif()

######################################编译对象######################################
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
utils/versionlog.cpp
widget.cpp
utils/customplaintextedit.cpp
utils/customplaintextedit.h
utils/doubleqprogressbar.cpp
utils/doubleqprogressbar.h
utils/versionlog.h
widget.h
xbot.cpp
xbot.h
xnet.cpp
xnet.h
xserver.cpp
xserver.h
xtool.cpp
xtool.h
xconfig.h
utils/versionlog.ui
widget.ui
utils/qmarkdowntextedit/qplaintexteditsearchwidget.ui
utils/qmarkdowntextedit/linenumberarea.h
utils/qmarkdowntextedit/markdownhighlighter.h
utils/qmarkdowntextedit/qmarkdowntextedit.h
utils/qmarkdowntextedit/qownlanguagedata.h
utils/qmarkdowntextedit/qplaintexteditsearchwidget.h
utils/qmarkdowntextedit/qmarkdowntextedit.cpp
utils/qmarkdowntextedit/markdownhighlighter.cpp
utils/qmarkdowntextedit/qmarkdowntextedit.cpp
utils/qmarkdowntextedit/qownlanguagedata.cpp
utils/qmarkdowntextedit/qplaintexteditsearchwidget.cpp
)
# 查找Qt相关库
find_package(Qt5 COMPONENTS Widgets Network Script REQUIRED)
#添加llama相关项目,将会强制应用父项目的编译设置
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(llama.cpp)
add_subdirectory(llama.cpp/examples/main)
add_subdirectory(llama.cpp/examples/llava)
add_subdirectory(llama.cpp/examples/server)
# 链接相关库,生成可执行文件
target_link_libraries(${TARGET} PRIVATE common llava llama ${EXTRA_LIBS} ${CMAKE_THREAD_LIBS_INIT} Qt5::Widgets Qt5::Network Qt5::Script)
target_compile_features(${TARGET} PRIVATE cxx_std_11)

