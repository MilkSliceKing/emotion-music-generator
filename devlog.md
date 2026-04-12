# 开发日志 (devlog.md)

---

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
   ```

3. **代码实现**
   - 编写main.cpp（面部检测主程序）
   - 实现FaceDetector类（面部检测器）
   - 添加CMakeLists.txt（构建配置）

4. **编译和运行**
   ```bash
   cd emotion-music-generator
   mkdir build && cd build
   cmake .. && make
   ./emotion_music_generator
   ```

#### 遇到的问题及解决方案：

- **问题**：摄像头无法访问
  - **解决**：检查虚拟机摄像头权限设置
- **问题**：dlib编译错误
  - **解决**：确保安装了正确的依赖包

#### 下一步计划：
- 添加情绪识别功能
- 实现音乐生成模块
- 优化性能

---

## 日期：2026-04-12

### 任务：完整项目骨架开发 + VM 环境部署

#### 一、开发方案讨论

**核心决策：在 Windows 上编写代码，还是直接在 VM 里开发？**

经过分析，决定采用以下方案：
- **在 Windows 上准备好源码文件**，通过 GitHub 推送
- **在 Ubuntu VM 里 git clone**，编译和运行全在 VM 上完成
- **使用 VS Code Remote-SSH** 连接 VM 进行编辑

选择理由：
- 项目依赖（OpenCV、dlib、PortAudio）都是 Linux 系统级库，Windows 和 Linux 的库路径完全不同
- 摄像头接口不同（V4L2 vs DirectShow），必须在 VM 上测试
- dlib 在 Windows 上编译困难，Ubuntu 上 `apt install` 即可
- 避免验收时出现"Windows 能跑但 VM 跑不了"的问题

#### 二、项目骨架创建（AI 辅助）

使用 Claude Code (GLM) 在 Windows 端生成完整项目骨架，共 22 个文件：

| 模块 | 文件 | 说明 |
|------|------|------|
| 构建 | CMakeLists.txt, build.sh, .gitignore, .gitattributes | 构建配置和项目管理 |
| 面部检测 | src/detector/face_detector.h/cpp | dlib HOG+SVM 人脸检测 + 68 关键点提取 |
| 情绪识别 | src/detector/emotion_recognizer.h/cpp | 基于关键点特征的情绪分类 |
| 情绪映射 | src/mapper/emotion_mapper.h/cpp | 情绪 → 音乐参数 (BPM/调性/音阶/力度) |
| 音乐生成 | src/generator/music_generator.h/cpp | 基于音阶的算法式旋律生成 |
| 音频播放 | src/audio/audio_player.h/cpp | PortAudio 正弦波合成播放 |
| 主程序 | src/main.cpp | 完整流水线 + 键盘控制 + FPS显示 |
| 配置 | config/config.yaml | 运行参数配置 |
| 文档 | docs/designed.md, perf.md, ai_usage.md | 设计/性能/AI使用文档 |

#### 三、新增模块技术细节

1. **情绪识别模块** (`emotion_recognizer`)
   - 基于dlib 68关键点提取4个特征：
     - MAR（嘴巴纵横比）：嘴巴张开程度
     - EAR（眼睛纵横比）：眼睛睁大程度
     - 眉毛高度：眉毛上扬/下压
     - 嘴角角度：微笑(负值)/悲伤(正值)
   - 基于阈值规则分类为7种情绪
   - 5帧滑动窗口取众数的平滑机制

2. **情绪映射模块** (`emotion_mapper`)
   - Happy → C大调 140BPM (明亮)
   - Sad → A小调 60BPM (忧郁)
   - Angry → E小调 160BPM (激烈)
   - Surprised → G大调 120BPM (俏皮)
   - Neutral → C大调 90BPM (平静)

3. **音乐生成模块** (`music_generator`)
   - 音阶随机游走：在音阶内以相邻音偏好生成旋律
   - 支持大调、小调、布鲁斯三种音阶
   - 力度在基准值 ±15 范围内随机化

4. **音频播放模块** (`audio_player`)
   - PortAudio 正弦波合成 + 简单 ADSR 包络
   - MIDI 音高 → 频率转换 (A4=440Hz)
   - 条件编译：未安装 PortAudio 时编译为桩模块

5. **主程序** (`main.cpp`)
   - 完整流水线：检测 → 识别 → 映射 → 生成 → 播放
   - 自动每5秒根据当前情绪播放一段旋律
   - 键盘控制：ESC退出、SPACE手动播放、M切换自动播放
   - 实时 FPS 显示

#### 四、部署过程中遇到的问题

**问题1：Git 推送失败（GitHub 连接超时）**
```
fatal: unable to access 'https://github.com/...': Failed to connect to github.com port 443
```
- **原因**：国内网络无法直连 GitHub
- **解决**：配置 Git 代理
  ```bash
  git config --global http.proxy http://127.0.0.1:7897
  git config --global https.proxy http://127.0.0.1:7897
  ```

**问题2：Git push rejected（non-fast-forward）**
```
! [rejected] main -> main (non-fast-forward)
```
- **原因**：远程仓库有本地没有的提交（GitHub 创建仓库时自动生成）
- **解决**：`git pull --rebase` 后产生冲突，`git checkout --ours` 保留本地版本，最终 `git push --force`

**问题3：VM 里 git clone 失败**
```
GnuTLS recv error (-54): Error in the pull function
```
- **原因**：VM 也不能直连 GitHub
- **解决**：在 VM 里配置代理走 Windows 网络
  ```bash
  export https_proxy=http://Windows局域网IP:7897
  export http_proxy=http://Windows局域网IP:7897
  ```
  - 前提：Windows 代理软件开启"允许局域网连接"

**问题4：编译链接错误（BLAS 缺失）**
```
/usr/bin/ld: undefined reference to symbol 'cblas_dgemm'
error adding symbols: DSO missing from command line
```
- **原因**：dlib 依赖 BLAS/LAPACK 数学库，CMakeLists.txt 未链接
- **解决**：在 CMakeLists.txt 中添加
  ```cmake
  find_package(BLAS REQUIRED)
  find_package(LAPACK REQUIRED)
  # 并在 target_link_libraries 中添加 ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES}
  ```

**问题5：摄像头无法识别**
```
OpenCV | GStreamer warning: Cannot identify device '/dev/video0'
```
- **原因**：VMware 未将 USB 摄像头映射进虚拟机
- **解决**：VMware 菜单 → VM → Removable Devices → 选择摄像头 → Connect

**问题6：模型文件找不到**
```
Unable to open models/shape_predictor_68_face_landmarks.dat for reading
```
- **原因**：从 `build/` 目录运行时，相对路径解析到 `build/models/`
- **解决**：从项目根目录运行 `./build/emotion_music_generator`

#### 五、最终运行结果

程序成功在 Ubuntu VM 上运行：
- 摄像头正常打开 (640x480)
- dlib 68 关键点模型加载成功
- 面部检测功能正常工作

#### 六、下一步计划

- [ ] 调优情绪识别阈值（当前为通用参考值，需根据实际效果调整）
- [ ] 测试完整流水线（检测→识别→映射→生成→播放）
- [ ] 记录性能数据到 perf.md（FPS、延迟等）
- [ ] 安装 PortAudio 测试音频播放功能
- [ ] 优化帧率和延迟
