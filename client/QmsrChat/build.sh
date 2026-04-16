#!/bin/bash

# ==============================================================================
# QmsrChat Linux Build Script
# 用于在 Linux (Ubuntu/WSL2) 下构建 QmsrChat 客户端
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "=========================================="
echo "QmsrChat Linux Build Script"
echo "=========================================="
echo "Build Type: ${BUILD_TYPE}"
echo "Script Directory: ${SCRIPT_DIR}"
echo ""

# 切换到脚本目录
cd "${SCRIPT_DIR}"

# 创建构建目录
BUILD_DIR="${SCRIPT_DIR}/build"
if [ -d "${BUILD_DIR}" ]; then
    echo "清理旧的构建目录..."
    rm -rf "${BUILD_DIR}"
fi

echo "创建构建目录: ${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 配置 CMake
echo ""
echo "=========================================="
echo "配置 CMake..."
echo "=========================================="
cmake .. \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_C_COMPILER=gcc

# 构建项目
echo ""
echo "=========================================="
echo "编译项目..."
echo "=========================================="
cmake --build . -j$(nproc)

# 后处理：复制配置文件
echo ""
echo "=========================================="
echo "后处理..."
echo "=========================================="
if [ -f "${SCRIPT_DIR}/config.ini" ]; then
    echo "复制配置文件到构建目录..."
    cp "${SCRIPT_DIR}/config.ini" "${BUILD_DIR}/"
else
    echo "警告: config.ini 不存在，跳过复制"
fi

# 完成
echo ""
echo "=========================================="
echo "构建完成！"
echo "=========================================="
echo "可执行文件位置: ${BUILD_DIR}/QmsrChat"
echo ""
echo "运行应用程序:"
echo "  cd ${BUILD_DIR}"
echo "  ./QmsrChat"
echo ""

# 显示构建输出目录内容
echo "构建目录内容:"
ls -lh "${BUILD_DIR}" | grep -E "QmsrChat|config|Total"
