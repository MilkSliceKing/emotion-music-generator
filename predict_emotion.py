#!/usr/bin/env python3
"""情绪识别桥接脚本 — 支持 daemon 模式和单次调用模式

daemon 模式 (无参数):
  启动时加载模型，输出 READY，然后循环从 stdin 读取图片路径，
  分析后输出结果到 stdout，收到 EXIT 退出。

单次调用模式 (传图片路径参数):
  兼容旧行为，分析一张图片后退出。
"""
import sys
import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'

from deepface import DeepFace


def analyze_image(img_path):
    """分析单张图片，返回格式化结果字符串"""
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

    # 输出格式: OK,情绪标签,angry值,disgust值,fear值,happy值,sad值,surprise值,neutral值
    values = (f"{emo.get('angry',0):.2f},{emo.get('disgust',0):.2f},"
              f"{emo.get('fear',0):.2f},{emo.get('happy',0):.2f},"
              f"{emo.get('sad',0):.2f},{emo.get('surprise',0):.2f},"
              f"{emo.get('neutral',0):.2f}")
    return f"OK,{dominant},{values}"


def daemon_mode():
    """守护进程模式：模型只加载一次，通过 stdin/stdout 通信"""
    # 模型已在 import 时预加载
    sys.stdout.write("READY\n")
    sys.stdout.flush()

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        if line == "EXIT":
            sys.stdout.write("BYE\n")
            sys.stdout.flush()
            break

        try:
            result = analyze_image(line)
            sys.stdout.write(result + "\n")
        except Exception as e:
            sys.stdout.write(f"ERROR,{str(e)}\n")
        sys.stdout.flush()


def single_mode(img_path):
    """单次调用模式：兼容旧接口"""
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

        # 旧格式: 情绪标签,angry值,...,neutral值
        values = (f"{emo.get('angry',0):.2f},{emo.get('disgust',0):.2f},"
                  f"{emo.get('fear',0):.2f},{emo.get('happy',0):.2f},"
                  f"{emo.get('sad',0):.2f},{emo.get('surprise',0):.2f},"
                  f"{emo.get('neutral',0):.2f}")
        print(f"{dominant},{values}")

    except Exception as e:
        print(f"ERROR,{str(e)}")
        sys.exit(1)


if __name__ == "__main__":
    if len(sys.argv) >= 2:
        # 单次调用模式（兼容旧接口）
        single_mode(sys.argv[1])
    else:
        # 无参数 → daemon 模式
        daemon_mode()
