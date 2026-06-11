# 性能报告 (perf.md)

---

## 测试环境

| 项目 | 配置 |
|------|------|
| 操作系统 | Ubuntu 22.04 (VMware/VirtualBox 虚拟机) |
| CPU | 宿主机分配 4 核 |
| 内存 | 宿主机分配 4 GB |
| 摄像头 | USB 摄像头透传（VMware/VirtualBox USB 映射） |
| 编译模式 | Release (-O3 -march=native) |

---

## 性能指标

### 优化前（初代版本：DeepFace Python 桥接 + popen 每次新建进程）

| 指标 | 数值 |
|------|------|
| 程序启动时间 | ~10s（Python + TensorFlow 模型加载） |
| 单次情绪识别耗时 | 1-2s（每次 popen 新建 Python 进程 + 模型加载） |
| 摄像头帧率（含推理） | ~1 FPS |
| 音乐生成耗时 | <50ms |
| 音频播放延迟 | ~100ms |
| 内存占用 | ~1-2GB（TensorFlow 运行时） |

### 优化后（最终版本：onnxruntime 纯 C++ 推理）

| 指标 | 数值 | 优化方法 |
|------|------|---------|
| 程序启动时间 | <1s | 移除 Python 依赖，纯 C++ onnxruntime 加载 |
| 单次情绪识别耗时 | ~10-30ms | onnxruntime C++ API，无需跨进程通信 |
| 摄像头帧率（含推理） | ~15-25 FPS | 纯 C++ 推理无进程开销 + Release -O3 优化 |
| 音乐生成耗时 | <50ms | 和弦进行缓存 + 算法优化 |
| 音频播放延迟 | ~100ms | paplay WAV 播放方案 |
| 内存占用 | ~200-300MB | 仅 onnxruntime + OpenCV + dlib |

---

## 优化记录

| 日期 | 优化措施 | 效果 | Commit |
|------|---------|------|--------|
| 2026-04-13 | 初代版本：DeepFace Python popen 桥接 | 基线：1-2s/次推理 | — |
| 2026-04-16 | pipe+fork 常驻 daemon（模型只加载一次） | 推理 0.1-0.3s/次（~10x 加速） | da3a118 |
| 2026-04-16 | OpenCV DNN + FERPlus ONNX 纯 C++ 方案 | 启动 <1s，但全输出 Neutral（模型偏置） | 128bcc7 |
| 2026-04-21 | 换用 HSEmotion 模型 + onnxruntime | 准确识别 + 纯 C++ 推理 ~10-30ms | 29de11c |
| 2026-04-21 | Release 模式 -O3 编译优化 | 帧率提升约 30% | cdfc445 |
| 2026-04-26 | 修复 BGR→RGB 颜色通道 bug | 摄像头模式识别准确率大幅提升 | 7db457c |

---

## 情绪识别准确率测试

使用 `emotion_test` 工具对 21 张测试图片进行批量测试：

| 指标 | 数值 |
|------|------|
| 总体准确率 | 85%（18/21 正确） |
| Happy 准确率 | 较高 |
| Angry 准确率 | 中等（与 Disgust 存在混淆） |
| Disgust 准确率 | 较低（与 Angry 互相混淆） |
| Surprise 准确率 | 中等（部分误判为 Fear） |
| Neutral 准确率 | 较高 |

---

## 架构演进对性能的影响

```
初代 (DeepFace popen)     → 1-2s/次, ~1 FPS, 1-2GB 内存
  ↓ pipe+fork daemon       → 0.1-0.3s/次, ~3-5 FPS
  ↓ ONNX 纯 C++ 推理       → 10-30ms/次, ~15-25 FPS, 200-300MB 内存
  ↓ Release -O3 编译       → 进一步提升
```

总体提升：**推理速度 ~100x，内存 ~5x，帧率 ~15x**。
