###################################### 基础配置 ######################################
cmake_minimum_required(VERSION 3.12)
project(body)
enable_testing()
set(EVA_TARGET eva)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(version b7415)

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

add_subdirectory(tests)

if (EVA_ENABLE_COVERAGE)
    find_program(GCOVR_EXECUTABLE gcovr)
    if (GCOVR_EXECUTABLE)
        set(EVA_COVERAGE_DIR ${CMAKE_BINARY_DIR}/reports/coverage)
        set(EVA_COVERAGE_HTML ${EVA_COVERAGE_DIR}/index.html)
        set(EVA_COVERAGE_XML ${EVA_COVERAGE_DIR}/coverage.xml)
        add_custom_target(coverage
                          COMMAND ${CMAKE_COMMAND} -E make_directory ${EVA_COVERAGE_DIR}
                          COMMAND ${GCOVR_EXECUTABLE}
                                  --root ${CMAKE_SOURCE_DIR}
                                  --object-directory ${CMAKE_BINARY_DIR}
                                  --print-summary
                                  --html-details ${EVA_COVERAGE_HTML}
                                  --xml ${EVA_COVERAGE_XML}
                                  --filter ${CMAKE_SOURCE_DIR}/src
                                  --filter ${CMAKE_SOURCE_DIR}/tests
                                  --exclude ".*/CMakeFiles/.+_autogen/.*"
                                  --exclude ".*/qrc_.*"
                          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                          COMMENT "Generating gcovr coverage report (reports/coverage/index.html + coverage.xml)")
    else()
        message(WARNING "gcovr executable not found; 'coverage' target will not be available.")
    endif()
endif()
