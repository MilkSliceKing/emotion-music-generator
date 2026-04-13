#!/usr/bin/env python3
"""快速测试 FER+ ONNX 模型，找出正确的预处理方式"""
import numpy as np
import cv2
import sys

MODEL_PATH = "models/emotion-ferplus.onnx"
LABELS = ["Neutral", "Happy", "Surprise", "Sad", "Angry", "Disgust", "Fear", "Contempt"]

if len(sys.argv) < 2:
    print("用法: python3 test_emotion.py 图片路径")
    sys.exit(1)

img = cv2.imread(sys.argv[1])
if img is None:
    print(f"无法读取: {sys.argv[1]}")
    sys.exit(1)

# 用 SSD 检测人脸
net_det = cv2.dnn.readNetFromCaffe(
    "models/face_detector/deploy.prototxt",
    "models/face_detector/res10_300x300_ssd_iter_140000.caffemodel"
)

blob_det = cv2.dnn.blobFromImage(img, 1.0, (300, 300), (104, 177, 123))
net_det.setInput(blob_det)
dets = net_det.forward()

best_conf = 0
best_box = None
for i in range(dets.shape[2]):
    conf = dets[0, 0, i, 2]
    if conf > best_conf:
        best_conf = conf
        best_box = dets[0, 0, i, 3:7]

if best_box is None:
    print("未检测到人脸!")
    sys.exit(1)

h, w = img.shape[:2]
x1 = int(best_box[0] * w)
y1 = int(best_box[1] * h)
x2 = int(best_box[2] * w)
y2 = int(best_box[3] * h)

face = img[y1:y2, x1:x2]
gray = cv2.cvtColor(face, cv2.COLOR_BGR2GRAY)
resized = cv2.resize(gray, (64, 64))

print(f"检测到人脸 (置信度: {best_conf:.2f})")
print(f"人脸区域: ({x1},{y1})-({x2},{y2})")
print()

# 加载情绪模型
net_emo = cv2.dnn.readNetFromONNX(MODEL_PATH)

# 测试 4 种不同的预处理
tests = [
    ("方式A: [0,255] 不归一化",          lambda r: cv2.dnn.blobFromImage(r, 1.0,   (64,64), (0,), True, False)),
    ("方式B: [0,1] 除以255",             lambda r: cv2.dnn.blobFromImage(r, 1/255.0,(64,64), (0,), True, False)),
    ("方式C: [-1,1] 减128除128",         lambda r: (cv2.dnn.blobFromImage(r, 1.0, (64,64), (0,), True, False) - 128) / 128),
    ("方式D: 标准化 mean=0.5 std=0.5",   lambda r: (cv2.dnn.blobFromImage(r, 1/255.0,(64,64),(0,),True,False) - 0.5) / 0.5),
]

for name, preprocess in tests:
    blob = preprocess(resized)
    net_emo.setInput(blob)
    output = net_emo.forward()
    probs = output[0]

    # Top 3
    top3 = np.argsort(probs)[::-1][:3]
    print(f"--- {name} ---")
    for idx in top3:
        print(f"  {LABELS[idx]:10s}: {probs[idx]*100:6.2f}%")
    print()
