#!/bin/bash

# 获取脚本所在目录的上一级目录作为工作目录
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
parent_dir="$(dirname "$script_dir")"
cd "$parent_dir" || exit 1

# 默认构建目录名称
BUILD_NAME="build"

# 默认CMake参数
CMAKE_OPTIONS=(-DBODY_PACK=ON -DGGML_VULKAN=OFF -DGGML_CUDA=OFF)

# 解析参数
while [[ $# -gt 0 ]]; do
    case "$1" in
        -o|--options) 
            shift
            # 直接收集所有后续参数直到结束
            while [[ $# -gt 0 ]]; do
                CMAKE_OPTIONS+=("$1")
                shift
            done
            ;;
        *)
            # 第一个非选项参数视为构建目录名
            if [[ "$BUILD_NAME" == "build" ]]; then
                BUILD_NAME="$1"
            else
                echo "警告: 忽略额外参数: $1"
            fi
            shift
            ;;
    esac
done

# 自动更新仓库
echo "更新代码仓库..."
if git pull; then
    echo "代码更新成功"
else
    echo "警告: 代码更新失败，继续构建..."
fi

# 配置项目
echo "配置项目 (构建目录: ${BUILD_NAME})..."
echo "CMake参数: ${CMAKE_OPTIONS[*]}"
cmake -B "${BUILD_NAME}" "${CMAKE_OPTIONS[@]}"

# 构建项目（使用16线程并行编译）
echo "开始构建..."
cmake --build "${BUILD_NAME}" --config=Release -j 16