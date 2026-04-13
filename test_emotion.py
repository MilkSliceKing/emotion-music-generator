#!/usr/bin/env python3
"""快速测试 FER+ ONNX 情绪模型，找出正确的预处理方式"""
import numpy as np
import cv2
import sys

MODEL_PATH = "models/emotion-ferplus.onnx"
LABELS = ["Neutral", "Happy", "Surprise", "Sad", "Angry", "Disgust", "Fear", "Contempt"]

if len(sys.argv) < 2:
    print("用法: python3 test_emotion.py 图片路径 [x y w h]")
    print("  不指定坐标时用 Haar 级联自动检测人脸")
    sys.exit(1)

img = cv2.imread(sys.argv[1])
if img is None:
    print(f"无法读取: {sys.argv[1]}")
    sys.exit(1)

# 检测人脸（使用 OpenCV 内置的 Haar 级联）
if len(sys.argv) >= 6:
    x, y, w, h = int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
else:
    gray_full = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    cascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
    faces = cascade.detectMultiScale(gray_full, 1.1, 5)
    if len(faces) == 0:
        print("未检测到人脸! 请手动指定坐标: python3 test_emotion.py 图片 x y w h")
        sys.exit(1)
    x, y, w, h = faces[0]

# 裁剪人脸，加边距
margin = 0.3
mx, my = int(w * margin), int(h * margin)
x1 = max(0, x - mx)
y1 = max(0, y - my)
x2 = min(img.shape[1], x + w + mx)
y2 = min(img.shape[0], y + h + my)

face = img[y1:y2, x1:x2]
gray = cv2.cvtColor(face, cv2.COLOR_BGR2GRAY)
resized = cv2.resize(gray, (64, 64))

print(f"人脸区域: ({x1},{y1})-({x2},{y2}), 尺寸: {face.shape}")
print()

# 加载情绪模型
net = cv2.dnn.readNetFromONNX(MODEL_PATH)

# 测试 4 种预处理
tests = [
    ("A: [0,255] 不归一化",        lambda r: cv2.dnn.blobFromImage(r, 1.0,    (64,64), (0,), True, False)),
    ("B: [0,1] 除以255",           lambda r: cv2.dnn.blobFromImage(r, 1/255.0, (64,64), (0,), True, False)),
    ("C: [-1,1] 减128除128",       lambda r: (cv2.dnn.blobFromImage(r, 1.0, (64,64), (0,), True, False) - 128) / 128),
    ("D: 标准化 mean=0.5 std=0.5", lambda r: (cv2.dnn.blobFromImage(r, 1/255.0,(64,64),(0,),True,False) - 0.5) / 0.5),
]

for name, preprocess in tests:
    blob = preprocess(resized)
    net.setInput(blob)
    output = net.forward()
    probs = output[0]

    top3 = np.argsort(probs)[::-1][:3]
    print(f"--- {name} ---")
    for idx in top3:
        print(f"  {LABELS[idx]:10s}: {probs[idx]*100:6.2f}%")
    print()
