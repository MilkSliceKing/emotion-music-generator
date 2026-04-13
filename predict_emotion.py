#!/usr/bin/env python3
"""情绪识别桥接脚本 — 被 C++ 程序调用
输入: 人脸裁剪图片路径
输出: 情绪标签和置信度（stdout，逗号分隔）
"""
import sys
import json
import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'  # 关闭 TensorFlow 日志

from deepface import DeepFace

if len(sys.argv) < 2:
    print("ERROR,no_image")
    sys.exit(1)

img_path = sys.argv[1]

try:
    result = DeepFace.analyze(
        img_path=img_path,
        actions=['emotion'],
        enforce_detection=False,
        silent=True
    )
    if isinstance(result, list):
        result = result[0]

    emo = result['emotion']
    dominant = result['dominant_emotion']

    # 输出格式: 情绪标签,angry值,disgust值,fear值,happy值,sad值,surprise值,neutral值
    values = f"{emo.get('angry',0):.2f},{emo.get('disgust',0):.2f},{emo.get('fear',0):.2f}," \
             f"{emo.get('happy',0):.2f},{emo.get('sad',0):.2f},{emo.get('surprise',0):.2f}," \
             f"{emo.get('neutral',0):.2f}"
    print(f"{dominant},{values}")

except Exception as e:
    print(f"ERROR,{str(e)}")
    sys.exit(1)
