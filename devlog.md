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