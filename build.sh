#!/bin/bash
# 一键构建脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "=== 情绪音乐生成器 - 构建脚本 ==="

# 检查依赖
echo "[1/4] 检查依赖..."
dpkg -l | grep -q libopencv-dev || { echo "错误: 未安装 OpenCV (sudo apt install libopencv-dev)"; exit 1; }
dpkg -l | grep -q libdlib-dev || { echo "错误: 未安装 dlib (sudo apt install libdlib-dev)"; exit 1; }

# 创建构建目录
echo "[2/4] 创建构建目录..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# CMake 配置
echo "[3/4] CMake 配置..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# 编译
echo "[4/4] 编译..."
make -j$(nproc)

echo ""
echo "=== 构建完成 ==="
echo "运行: cd build && ./emotion_music_generator"
echo ""
echo "注意: 首次运行需要下载 landmark 模型:"
echo "  wget http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2"
echo "  bzip2 -d shape_predictor_68_face_landmarks.dat.bz2"
echo "  mv shape_predictor_68_face_landmarks.dat models/"
