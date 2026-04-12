 # 开发日志 (devlog.md)

  ## 日期：2026-04-11

  ### 任务：创建初代程序 - 面部检测功能

  #### 步骤记录：

  1. **环境准备**
     - 在Ubuntu终端中更新包列表：`sudo apt update`
     - 安装OpenCV：`sudo apt install libopencv-dev`
     - 安装dlib和编译工具：`sudo apt install libdlib-dev build-essential cmake`

  2. **项目结构创建**
     ```bash
     mkdir -p emotion-music-generator/src/detector
     touch emotion-music-generator/src/main.cpp
     touch emotion-music-generator/src/detector/face_detector.h
     touch emotion-music-generator/src/detector/face_detector.cpp

  3. 代码实现
    - 编写main.cpp（面部检测主程序）
    - 实现FaceDetector类（面部检测器）
    - 添加CMakeLists.txt（构建配置）
  4. 编译和运行
  cd emotion-music-generator
  mkdir build
  cd build
  cmake ..
  make
  ./emotion_music_generator

  遇到的问题及解决方案：

  - 问题：摄像头无法访问
    - 解决：检查虚拟机摄像头权限设置
  - 问题：dlib编译错误
    - 解决：确保安装了正确的依赖包

  下一步计划：

  - 添加情绪识别功能
  - 实现音乐生成模块
  - 优化性能

  - ## 日期：2026-04-12

### 任务：添加情绪识别 + 音乐生成 + 音频播放（完整流水线）

#### 新增模块：

1. **情绪识别模块** (`src/detector/emotion_recognizer.h/cpp`)
   - 基于dlib 68关键点的特征提取
   - 实现特征：嘴巴纵横比(MAR)、眼睛纵横比(EAR)、眉毛高度、嘴角角度
   - 基于阈值规则的情绪分类：Happy/Sad/Angry/Surprised/Neutral/Fear/Disgust
   - 情绪平滑机制（5帧滑动窗口取众数）

2. **情绪映射模块** (`src/mapper/emotion_mapper.h/cpp`)
   - 情绪 → 音乐参数映射
   - 参数包括：BPM、调性、音阶(major/minor/blues)、力度、情绪标签

3. **音乐生成模块** (`src/generator/music_generator.h/cpp`)
   - 基于音阶的算法式旋律生成
   - 支持大调、小调、布鲁斯音阶
   - 相邻音偏好策略，生成更自然的旋律

4. **音频播放模块** (`src/audio/audio_player.h/cpp`)
   - PortAudio 实时音频输出
   - 正弦波合成 + ADSR包络
   - MIDI音高 → 频率转换
   - 编译时检测 PortAudio，未安装时编译为桩模块

5. **主程序更新** (`src/main.cpp`)
   - 完整流水线：检测 → 识别 → 映射 → 生成 → 播放
   - 实时FPS显示
   - 键盘控制：ESC退出、SPACE手动播放、M切换自动播放
   - 自动每5秒根据当前情绪播放一段旋律

#### 构建配置更新：
- CMakeLists.txt：条件编译 PortAudio 支持
- 新增 config.yaml 配置文件
- 新增 build.sh 一键构建脚本

#### 下一步计划：

- 在虚拟机中编译测试完整流水线
- 调优情绪识别阈值
- 记录性能数据到 perf.md
- 优化帧率和延迟
