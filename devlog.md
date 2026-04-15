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

#### 二、项目骨架创建（AI 辅助）

使用 Claude Code (GLM) 在 Windows 端生成完整项目骨架，共 22 个文件。

#### 三、部署过程中遇到的问题

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| Git push 连接超时 | 国内网络无法直连 GitHub | 配置代理 `git config --global http.proxy http://127.0.0.1:7897` |
| Git push rejected | 远程有本地没有的提交 | `git pull --rebase` + `git push --force` |
| VM 里 git clone 失败 | VM 也不能直连 GitHub | VM 里配代理走 Windows 网络 |
| 编译链接错误 cblas_dgemm | dlib 依赖 BLAS/LAPACK | CMakeLists.txt 添加 `find_package(BLAS/LAPACK)` |
| 摄像头无法识别 | VMware USB 映射 | VMware Removable Devices → Connect |
| 模型文件找不到 | 从 build/ 目录运行路径错误 | 从项目根目录运行 |

#### 四、最终运行结果

程序成功在 Ubuntu VM 上运行，面部检测和68关键点提取功能正常。

---

## 日期：2026-04-13

### 任务：情绪识别模型攻关 — 从失败到成功

#### 一、初步尝试：规则判断 + FER+ ONNX 模型

**尝试1：基于68关键点的规则判断**
- 提取 MAR（嘴巴纵横比）、EAR（眼睛纵横比）、眉毛高度、嘴角角度
- 用固定阈值分类为7种情绪
- **结果：不准确**，阈值是通用参考值，对不同人脸效果差异大

**尝试2：换用 FER+ ONNX 深度学习模型**
- 从 ONNX Model Zoo 下载 `emotion-ferplus-8.onnx`（34MB）
- 用 OpenCV DNN 模块加载
- **结果：所有表情都被识别为 Neutral**

#### 二、排查情绪识别不准的原因

这是当天最耗时的环节，经历了多轮排查：

**排查1：怀疑预处理归一化问题**
- 写了 Python 测试脚本 `test_emotion.py`，对同一张脸测试4种不同归一化方式：
  - A: [0,255] 不归一化
  - B: [0,1] 除以255
  - C: [-1,1] 减128除128
  - D: 标准化 mean=0.5 std=0.5
- **结果：4种方式都输出 Neutral**
- 结论：问题不在预处理，在模型本身

**排查2：怀疑 FER+ 模型不兼容 OpenCV DNN**
- 尝试换用 Caffe 格式情绪模型
- 多个 GitHub 源都下载失败（404 Not Found）
- caffemodel 找不到可靠的下载源

**排查3：用 DeepFace（Python）做对照实验**
- 安装 `pip install deepface`
- 遇到 `tf-keras` 缺失报错：`pip install tf-keras` 修复
- 用明显愤怒的照片测试：
  ```
  'angry': 86.01, 'fear': 13.97, 'dominant_emotion': 'angry'
  ```
- **DeepFace 能准确识别！**
- 结论：FER+ ONNX 模型与 OpenCV DNN 存在兼容性问题，不是预处理的问题

#### 三、最终方案：C++ + DeepFace Python 桥接

既然 DeepFace 能准确识别，但项目需要 C++，最终采用**桥接方案**：

```
C++ 人脸检测 → 裁剪人脸保存为临时图片 → 调用 Python DeepFace 脚本 → 解析输出结果
```

**桥接脚本** `predict_emotion.py`：
- 输入：人脸裁剪图片路径
- 输出：`dominant_emotion,angry,disgust,fear,happy,sad,surprise,neutral`（逗号分隔的置信度）
- C++ 通过 `popen()` 调用并解析 stdout

**C++ 侧** `emotion_recognizer.cpp`：
1. 裁剪人脸区域 + 20% 边距
2. `cv::imwrite()` 保存到 `/tmp/emo_face_x.jpg`
3. `popen("python3 predict_emotion.py /tmp/emo_face_x.jpg")` 调用
4. 解析逗号分隔的输出
5. 删除临时文件

#### 四、人脸检测的波折

**尝试升级为 OpenCV DNN SSD 检测器**
- 下载了 `res10_300x300_ssd_iter_140000.caffemodel`（10MB）
- 手写了 `deploy.prototxt`，但与 caffemodel 不匹配导致运行时报错
- 多次尝试从 GitHub 下载正确的 prototxt，均 404
- **暂时回退到 dlib HOG 检测器**（对正脸可用，侧脸后续优化）

#### 五、遇到的问题汇总

| # | 问题 | 尝试 | 结果 |
|---|------|------|------|
| 1 | 规则判断不准 | 换 FER+ ONNX 模型 | 模型也全输出 Neutral |
| 2 | FER+ 输出全 Neutral | 测试4种归一化 | 都不行，模型不兼容 |
| 3 | 换 Caffe 模型 | 从 GitHub 下载 | 多个源 404 |
| 4 | SSD prototxt 不匹配 | 手写 prototxt | 层结构错误，运行崩溃 |
| 5 | SSD prototxt 下载 | GitHub raw URL | 404 |
| 6 | DeepFace 缺依赖 | pip install tf-keras | 修复，DeepFace 正常工作 |
| 7 | 摄像头频繁断开 | VMware USB 映射不持久 | 改用图片/视频文件输入 |

#### 六、最终架构

```
dlib HOG 人脸检测 → 68关键点可视化
         ↓
    裁剪人脸区域 → 保存临时图片
         ↓
    Python DeepFace 情绪识别（7种情绪 + 置信度）
         ↓
    C++ 解析结果 → 情绪映射 → 音乐生成 → 音频播放
```

#### 七、运行结果

- 人脸检测：dlib HOG，正脸检测正常
- 情绪识别：DeepFace 准确识别（愤怒照片 86% 置信度）
- 支持图片/视频/摄像头三种输入模式
- 完整流水线跑通

---

## 日期：2026-04-15

### 任务：音频播放模块调试 — 从无声到有声

#### 一、问题描述

端到端流水线已跑通（人脸检测 → 情绪识别 → 音乐生成），但实际运行时**没有声音输出**。

- VM 可以发出声音（调音量有提示音）
- 已连接电脑声卡
- 情绪识别结果正确（Angry → E minor 160BPM）
- 程序无报错，所有 `[AudioPlayer]` 日志正常

#### 二、排查过程

**发现1：VM 代码与 Windows 代码不同步**

| | Windows 端 | VM 端 |
|---|---|---|
| 播放方案 | paplay/aplay（写 WAV + fork 子进程） | PortAudio（直接 API 调用） |
| 条件编译 | 无 | `#ifdef HAS_PORTAUDIO` |
| 来源 | AI 生成的初版代码 | 手动在 VM 上修改的版本 |

VM 端使用了 PortAudio 方案，通过 ALSA Host API 播放，但 ALSA 输出没有正确路由到 PulseAudio。

```
PortAudio → ALSA "default" 设备 → 路由失败 → 无声音
PulseAudio → alsa_output.pci-0000_02_02.0.analog-stereo → 正常
```

**发现2：VMware 虚拟声卡掉线**

排查过程中发现 `aplay` 和 `paplay` 均无法播放测试音频：
- PulseAudio sink 存在但 `SUSPENDED` 状态
- 音量 100%、未静音
- 直接 `aplay -D hw:0,0` 也没声

**解决方案**：在 VMware 中移除声卡设备 → 重新添加 → 重启 VM，音频恢复。

**发现3：VM Git 代理失效**

VM 端 `git pull` 连接失败，代理配置为 `127.0.0.1:7897`（已不可用）。

**解决方案**：
```bash
git config --global --replace-all http.proxy http://10.38.70.118:7897
```
使用 Windows 宿主机 IP 作为代理地址。

#### 三、最终修复

将 VM 端 `audio_player.cpp` 从 PortAudio 方案切换为 paplay/aplay 方案（与 Windows 端一致）：

```
合成正弦波音符 → 写入 /tmp/emotion_music.wav → fork() → execlp("paplay") → 播放
```

同步方式：Windows 端推送 GitHub → VM 端 `git pull` → `cmake .. && make` → 运行

#### 四、运行结果

```bash
./build/emotion_music_generator image test_images/test9.jpg
# [情绪] 识别结果: Angry
# [AudioPlayer] 合成 8 个音符, 3.0 秒
# [AudioPlayer] 开始播放
# 成功发出声音！
```

**端到端流水线完整跑通**：
```
dlib 人脸检测 → DeepFace 情绪识别(Angry) → 情绪映射(E minor 160BPM)
    → 音乐生成(8个音符) → WAV 合成 → paplay 播放 → ✓ 有声音
```

#### 五、遇到的问题汇总

| # | 问题 | 原因 | 解决方案 |
|---|------|------|---------|
| 1 | 程序运行无声音 | PortAudio ALSA 通道未路由到 PulseAudio | 换用 paplay/aplay 方案 |
| 2 | aplay/paplay 也没声 | VMware 虚拟声卡掉线 | 移除并重新添加 VMware 声卡 |
| 3 | VM git pull 失败 | 代理 127.0.0.1:7897 不可用 | 改用 Windows IP 10.38.70.118:7897 |
| 4 | git config 多值冲突 | 旧的 proxy 配置残留 | `--replace-all` 覆盖 |

---

#### 下一步计划（可选优化）

- [ ] 用摄像头测试实时情绪识别
- [ ] 优化 DeepFace 调用速度（当前每次识别需 1-2 秒）
- [ ] 记录性能数据到 perf.md
