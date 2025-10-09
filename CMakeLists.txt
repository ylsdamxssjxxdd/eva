################################# 若要更换第三方库版本，先阅读tools/README_cmake.md ##############################

###################################### 基础配置 ######################################
cmake_minimum_required(VERSION 3.12)
project(body)
set(EVA_TARGET eva)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(version b6123)

# 输出目录（所有构建类型）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)

###################################### 拆分配置 ######################################
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(cmake/ProjectOptions.cmake)    # 选项 & 编译器/平台标志
include(cmake/ThirdParty.cmake)        # 第三方依赖
include(cmake/QtSetup.cmake)           # Qt 自动化 & 发现
include(cmake/EnvInfo.cmake)           # 生成环境信息 & cmakeconfig.h
include(cmake/Targets.cmake)
include(cmake/PostBuild.cmake)         # 打包与部署

