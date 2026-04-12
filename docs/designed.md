# 项目设计文档 (designed.md)

---

## 一、项目概述

**名称**：情绪音乐生成器 (Emotion Music Generator)

**目标**：通过摄像头实时捕捉用户面部表情，识别情绪状态（开心、悲伤、愤怒、惊讶等），并动态生成对应风格的音乐片段，实现"表情-音乐"的实时互动体验。

**技术栈**：C++17 + OpenCV + dlib + PortAudio + CMake

**运行环境**：Linux (Ubuntu) + VSCode + GitHub

---

## 二、系统架构

```
摄像头输入 → 面部检测 → 关键点提取 → 情绪识别 → 情绪映射 → 音乐生成 → 音频播放
   (OpenCV)    (dlib HOG)   (dlib 68pt)   (规则)    (查表)    (算法)    (PortAudio)
```

### 模块职责

| 模块 | 文件 | 职责 |
|------|------|------|
| 面部检测 | `detector/face_detector` | 使用dlib HOG+SVM检测人脸，提取68关键点 |
| 情绪识别 | `detector/emotion_recognizer` | 基于关键点特征计算，分类为7种情绪 |
| 情绪映射 | `mapper/emotion_mapper` | 将情绪转换为音乐参数（调性、节拍、力度） |
| 音乐生成 | `generator/music_generator` | 基于音阶和参数，算法式生成旋律 |
| 音频播放 | `audio/audio_player` | 使用PortAudio实时播放生成的音符序列 |

---

## 三、核心算法

### 3.1 面部关键点 (dlib 68-point)

| 区域 | 索引 | 用途 |
|------|------|------|
| 下巴轮廓 | 0-16 | 人脸边界 |
| 左眉 | 17-21 | 眉毛高度特征 |
| 右眉 | 22-26 | 眉毛高度特征 |
| 鼻梁 | 27-30 | 面部朝向 |
| 鼻翼 | 31-35 | 嘴部定位参考 |
| 左眼 | 36-41 | 眼睛开合度 |
| 右眼 | 42-47 | 眼睛开合度 |
| 外唇 | 48-59 | 嘴巴张合、嘴角角度 |
| 内唇 | 60-67 | 嘴巴精细形状 |

### 3.2 情绪特征

- **MAR (Mouth Aspect Ratio)**：嘴巴纵向/横向比值，反映嘴巴张开程度
- **EAR (Eye Aspect Ratio)**：眼睛纵向/横向比值，反映眼睛睁大程度
- **Eyebrow Height**：眉毛到眼睛的垂直距离，反映眉毛上扬/下压
- **Mouth Corner Angle**：嘴角相对嘴中心的角度，正值为下垂(悲伤)，负值为上扬(微笑)

### 3.3 音乐生成策略

- 基于音阶的随机游走：在音阶内以相邻音偏好生成旋律
- 支持3种音阶：Major (大调)、Minor (小调)、Blues (布鲁斯)
- 简单ADSR包络避免爆音

---

## 四、数据流

```
cv::Mat (摄像头帧)
    ↓
FaceDetector::detectFaces() → vector<FaceRect>
    ↓
FaceDetector::getLandmarks() → vector<cv::Point> (68点)
    ↓
EmotionRecognizer::recognize() → Emotion (枚举)
    ↓
EmotionMapper::mapToMusic() → MusicParams {tempo, key, scale, velocity}
    ↓
MusicGenerator::generate() → vector<Note> {pitch, duration, velocity}
    ↓
AudioPlayer::play() → 正弦波合成 → PortAudio 输出
```

---

## 五、设计原则

1. **先求糙再求精**：先用简单算法验证端到端流程，再逐步优化
2. **模块化**：每个模块独立编译，通过清晰接口交互
3. **条件编译**：PortAudio为可选依赖，未安装时编译为桩模块
4. **CMake构建**：统一管理依赖和编译选项，支持Debug/Release模式
