#!/bin/bash
# 下载情绪识别 ONNX 模型（FERPlus）
# 用法: bash download_models.sh

MODEL_DIR="models/emotion_detector"
MODEL_FILE="$MODEL_DIR/emotion-ferplus-8.onnx"
MODEL_URL="https://huggingface.co/onnxmodelzoo/emotion-ferplus-8/resolve/main/emotion-ferplus-8.onnx"

mkdir -p "$MODEL_DIR"

if [ -f "$MODEL_FILE" ]; then
    echo "模型已存在: $MODEL_FILE"
    exit 0
fi

echo "正在下载 FERPlus 情绪识别模型 (~34MB)..."
echo "URL: $MODEL_URL"

if command -v wget &> /dev/null; then
    wget -O "$MODEL_FILE" "$MODEL_URL"
elif command -v curl &> /dev/null; then
    curl -L -o "$MODEL_FILE" "$MODEL_URL"
else
    echo "错误: 需要 wget 或 curl"
    exit 1
fi

if [ -f "$MODEL_FILE" ]; then
    SIZE=$(stat -c%s "$MODEL_FILE" 2>/dev/null || stat -f%z "$MODEL_FILE" 2>/dev/null)
    echo "下载完成: $MODEL_FILE ($(( SIZE / 1024 / 1024 ))MB)"
else
    echo "下载失败"
    exit 1
fi
