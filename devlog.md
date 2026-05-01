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

## 日期：2026-04-16

### 任务：性能优化与架构演进 — 从 Python 桥接到纯 C++ 推理

#### 一、优化1：pipe+fork 常驻 daemon（main 分支，初代版本完善）

**问题**：每次情绪识别都 `popen()` 新建 Python 进程，DeepFace/TensorFlow 模型每次重新加载，单次识别耗时 1-2 秒。

**方案**：启动时 `fork()` 一个 Python 常驻子进程，通过 `pipe()` 双向管道通信，模型只加载一次。

```
C++ 父进程                        Python daemon 子进程
    │                                     │
    ├── fork() + execlp("python3") ──→  加载 DeepFace 模型
    │                                     │
    │   ←── pipe (等待 "READY")  ←────  stdout: "READY"
    │                                     │
    ├── write(图片路径) ──→ pipe ──→     stdin: 读取路径
    │                                     │
    │   ←── pipe ←────────────────────  stdout: "OK,Angry,..."
    │                                     │
    ├── write("EXIT") ──→ pipe ──→      退出
```

**改动文件**：

| 文件 | 改动 |
|------|------|
| `predict_emotion.py` | 新增 `daemon_mode()`，无参数启动时从 stdin 循环读取路径，输出 `OK,结果`；有参数时兼容旧的单次模式 |
| `src/detector/emotion_recognizer.cpp` | 新增 `startDaemon()` / `stopDaemon()`，用 `pipe()` + `fork()` + `dup2()` 管理子进程；识别时通过管道发送路径、读取结果；daemon 启动失败自动回退到旧的 `popen()` 单次模式 |
| `src/detector/emotion_recognizer.h` | 新增 `pipe_to_child_` / `pipe_from_child_` / `daemon_pid_` 成员，析构函数自动停止 daemon |

**效果**：
- 首次启动：模型加载 ~10s（仅一次）
- 后续每次推理：**0.1-0.3s**（原来 1-2s）
- 自动容错：daemon 启动失败自动回退 popen 模式

#### 二、优化2：OpenCV DNN + ONNX 替代 DeepFace（dev/opencv-dnn 分支）

**动机**：虽然 daemon 模式把推理延迟从 1-2s 降到 0.1-0.3s，但仍依赖 Python/TensorFlow/DeepFace，启动慢、依赖重。既然项目初版尝试 FER+ ONNX 模型时发现与 OpenCV DNN 存在兼容性问题（全输出 Neutral），但这次换用正确的方式重新尝试。

**方案**：用 FERPlus ONNX 模型 + OpenCV DNN 模块做纯 C++ 推理，彻底移除 Python 依赖。

**技术细节**：

```
输入: 人脸裁剪区域
  → cvtColor(BGR2Gray) → resize(64x64)
  → blobFromImage(1/255归一化) → [1,1,64,64] blob
  → net_.setInput(blob) → net_.forward()
  → 8 类概率输出 → 映射到项目 7 类 Emotion 枚举
```

**FERPlus 8 类 → 项目 7 类映射**：

| FERPlus 标签 | 项目 Emotion |
|-------------|-------------|
| Neutral | NEUTRAL |
| Happy | HAPPY |
| Surprise | SURPRISED |
| Sad | SAD |
| Anger | ANGRY |
| Disgust | DISGUST |
| Fear | FEAR |
| Contempt | NEUTRAL（归入中性） |

**改动文件**：

| 文件 | 改动 |
|------|------|
| `src/detector/emotion_recognizer.cpp` | 重写：移除所有 pipe/fork/popen/Python 代码，改用 `cv::dnn::readNetFromONNX()` 加载模型，`blobFromImage()` 预处理，`net_.forward()` 推理，FERPlus 8 类概率映射到 7 类 |
| `src/detector/emotion_recognizer.h` | 移除 daemon 相关成员，新增 `cv::dnn::Net net_` |
| `src/main.cpp` | `loadModel()` 参数从 Python 脚本路径改为 ONNX 模型路径 |
| `download_models.sh` | 新增：自动从 HuggingFace 下载 `emotion-ferplus-8.onnx`（~34MB） |
| `.gitignore` | 新增：忽略 `models/` 目录（大文件不入库） |

**预期性能对比**：

| 指标 | DeepFace daemon (初代) | OpenCV DNN + ONNX (二代) |
|------|----------------------|------------------------|
| 启动时间 | ~10s（加载 TF） | <1s（加载 ONNX） |
| 单次推理 | 0.1-0.3s | ~5-10ms |
| 依赖 | Python + TensorFlow + DeepFace | 仅 OpenCV（已安装） |
| 内存占用 | ~1-2GB（TF 运行时） | ~50-100MB |

#### 三、分支策略

```
main (初代版本)                    dev/opencv-dnn (二代优化)
  │                                    │
  ├── ... 音频修复等                    │
  ├── da3a118 perf: daemon 优化 ─────→ 128bcc7 feat: ONNX 替代
  │                                    │
  ✓ 初代完善版                    ✓ 纯 C++ 推理版（开发中）
```

初代版本在 main 分支稳定运行，二代优化在独立分支开发，互不影响。

#### 四、遇到的问题汇总

| # | 问题 | 解决方案 |
|---|------|---------|
| 1 | 每次 popen 新进程太慢 | fork 常驻 daemon，模型只加载一次 |
| 2 | daemon 管道读写需要正确处理换行符和缓冲 | `flush()` + 按 `\n` 分割读取 |
| 3 | daemon 可能启动失败 | 自动回退到 popen 单次模式 |
| 4 | 4/13 尝试 FER+ ONNX 全输出 Neutral | 这次正确处理了输入尺寸(64x64)和归一化(1/255) |

---

## 日期：2026-04-21

### 任务：二代 ONNX 推理版 VM 测试 — FERPlus 失败 → 换 HSEmotion 模型

#### 一、VM 测试 FERPlus ONNX 模型

在 VM 上首次完整测试 `dev/opencv-dnn` 分支。程序编译运行均通过，人脸检测正常，但情绪识别结果不理想。

**测试结果**：

```bash
# 单张测试
./build/emotion_music_generator image test_images/test9.jpg
# [情绪] 识别结果: Neutral

# 批量测试所有 test_images
for img in test_images/*.jpg; do
    echo "=== $img ==="
    ./build/emotion_music_generator image "$img" 2>&1 | grep "识别结果"
done
# 全部输出 Neutral（检测到人脸的图片）
```

**添加诊断输出**，查看模型原始 logits：

```
[DEBUG] output shape: [8 x 1] (dims: 2)
[DEBUG] raw output: Neutral=4.35 Happy=0.94 Surprise=-0.04 Sad=3.09 Anger=-0.20 Disgust=-3.03 Fear=-2.36 Contempt=-2.32
```

**分析**：
- 模型输出的是 logits（非概率），值域 [-3, 4.35]
- Neutral 总是最高值（4.35），argmax 一定选它
- 不同类的值确实有区分（不是全一样），说明模型在工作，但系统性地偏向 Neutral
- 4/13 初次尝试 FER+ 时也是同样的"全 Neutral"问题，说明 **FERPlus 模型对 Neutral 有固有偏置**

#### 二、决策：换用 HSEmotion 模型

经调研，选定 **HSEmotion（EmotiEffLib）** 作为替代模型：

| 对比项 | FERPlus (原模型) | HSEmotion enet_b0_8 (新模型) |
|--------|-----------------|------------------------------|
| 架构 | 简单 CNN | EfficientNet-B0（ImageNet 预训练 + VGGFace2 微调） |
| 训练数据 | FERPlus | AffectNet（更高质量） |
| 输入 | 64x64 灰度 | 224x224 RGB |
| 归一化 | 1/255 | ImageNet mean/std |
| 准确率 | ~55% (AffectNet) | ~61% (AffectNet 8类), AFEW 59.89% |
| 模型大小 | 34MB | 16MB |
| 标签顺序 | Neutral 首位 | Anger 首位 |

**来源**：GitHub av-savchenko/face-emotion-recognition（HSE 大学 + Sber AI Lab），ICML 2023 论文。

#### 三、代码改动

**3.1 download_models.sh** — 换模型下载地址

```diff
- MODEL_FILE="$MODEL_DIR/emotion-ferplus-8.onnx"
- MODEL_URL="https://huggingface.co/onnxmodelzoo/emotion-ferplus-8/resolve/main/emotion-ferplus-8.onnx"
+ MODEL_FILE="$MODEL_DIR/enet_b0_8_best_afew.onnx"
+ MODEL_URL="https://github.com/HSE-asavchenko/face-emotion-recognition/raw/main/models/affectnet_emotions/onnx/enet_b0_8_best_afew.onnx"
```

**3.2 emotion_recognizer.cpp** — 核心改动

1. **标签映射**：FERPlus 8 类 → HSEmotion 8 类（顺序完全不同）
   ```
   HSEmotion: {0:Anger, 1:Contempt, 2:Disgust, 3:Fear, 4:Happiness, 5:Neutral, 6:Sadness, 7:Surprise}
   ```

2. **预处理**：64x64 灰度 1/255 → 224x224 RGB + ImageNet 归一化
   ```cpp
   // blobFromImage 做均值减法: output = (img/255 - mean)
   cv::Scalar mean_pixel(0.485*255, 0.456*255, 0.406*255);
   cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0/255.0, cv::Size(224,224), mean_pixel, false);
   // 手动除以 std（blobFromImage 不支持 per-channel std）
   for (int c = 0; c < 3; ++c) {
       cv::Mat channel(224, 224, CV_32F, blob.ptr<float>(0, c));
       channel /= std_vals[c];  // {0.229, 0.224, 0.225}
   }
   ```

3. **输出解析**：统一用 `float* data` 直接访问，兼容 [1,8] 和 [8,1] 两种 shape；添加 softmax 转换得到真实概率。

**3.3 main.cpp** — 换模型路径

```diff
- recognizer.loadModel("models/emotion_detector/emotion-ferplus-8.onnx")
+ recognizer.loadModel("models/emotion_detector/enet_b0_8_best_afew.onnx")
```

#### 四、OpenCV 4.5.4 不兼容 → 换 onnxruntime

HSEmotion ONNX 模型在 VM 的 OpenCV 4.5.4 上加载失败：
```
Node [Identity] parse error: (-215:Assertion failed) inputs.size()
```

OpenCV 4.5.4 的 DNN 模块不支持 EfficientNet 使用的 `Identity` 节点。

**解决方案**：用 **onnxruntime C++ API** 替代 OpenCV DNN：
- 安装 onnxruntime v1.17.1（~30MB 共享库）
- 修改 CMakeLists.txt 添加 onnxruntime 查找和链接
- 重写 emotion_recognizer.h/cpp 使用 `Ort::Session` API
- 预处理改用 OpenCV split 通道逐个归一化，构造 NCHW tensor

**改动文件**：

| 文件 | 改动 |
|------|------|
| `CMakeLists.txt` | 移除 OpenCV DNN 依赖，添加 onnxruntime 查找和链接 |
| `src/detector/emotion_recognizer.h` | 替换 `cv::dnn::Net` 为 `Ort::Env` / `Ort::Session` |
| `src/detector/emotion_recognizer.cpp` | 用 onnxruntime API 重写：CreateTensor + Session::Run |

#### 五、VM 测试结果（HSEmotion + onnxruntime）

```bash
for img in test_images/face.jpg test_images/test*.jpg; do
    echo "=== $img ===" && ./build/emotion_music_generator image "$img" 2>&1 | grep "识别结果"
done
# face.jpg  → Neutral
# test3.jpg → Happy   ✓
# test4.jpg → Neutral
# test5.jpg → Angry   ✓
# test8.jpg → Happy   ✓
# test9.jpg → Angry   ✓
```

**识别准确！不再全 Neutral！** dev/opencv-dnn 已合并到 main。

#### 六、情绪识别方案演进史（最终版）

```
规则判断(68关键点阈值) → 不准
    ↓
FER+ ONNX 模型 → 全输出 Neutral（4/13 和 4/21 两次确认）
    ↓
排查 4 种归一化 → 都不行，模型固有偏置
    ↓
换 Caffe FER 模型 → 下载不到
    ↓
DeepFace Python → angry 86% ✓
    ↓
C++ + Python 桥接 (初代方案) → 可用但有 Python 依赖
    ↓
HSEmotion ONNX + OpenCV DNN → OpenCV 4.5.4 不兼容
    ↓
HSEmotion ONNX + onnxruntime → ✓ 纯 C++，识别准确！
```

---

## 日期：2026-04-21（续）

### 任务：UI 优化 — 从朴素文本到专业界面

#### 一、背景

原有 UI 使用裸 `cv::putText` 在画面左上角堆叠文字，无背景面板、无颜色区分、无可视化图表。信息杂乱且在浅色背景上可读性差。

#### 二、整体布局

```
+--------------------------------------------------+
| [HEADER BAR] y=0~55  半透明深色背景                 |
| FPS | ● PLAYING | AUTO:ON |  Emotion: Happy      |
+--------------------------------------------------+
|            主画面（人脸四角框 + 浅蓝关键点）          |
+---------------------------+------------------------+
| [CONFIDENCE PANEL]        | [MUSIC PANEL]          |
| 7条彩色条形图              | Key/Tempo + 均衡器动画  |
+---------------------------+------------------------+
| [FOOTER BAR]  "ESC:Quit | SPACE:Play | M:Toggle"  |
+--------------------------------------------------+
```

#### 三、新增文件

| 文件 | 说明 |
|------|------|
| `src/ui/overlay_renderer.h` | OverlayRenderer 类声明 |
| `src/ui/overlay_renderer.cpp` | ~230 行，所有 UI 绘制逻辑封装 |

#### 四、UI 元素详情

**4.1 半透明面板** — 用 `cv::addWeighted` 实现，4 个区域各有独立 alpha 值

**4.2 情绪颜色映射**（贯穿全局）：
```
Angry=红, Disgust=暗青, Fear=紫, Happy=黄, Sad=蓝, Surprised=橙, Neutral=灰白
```

**4.3 顶部状态栏**：FPS + 绿色/灰色播放指示器圆点 + AUTO:ON/OFF + 情绪标签（动态颜色）

**4.4 置信度面板**（左下角 250x164）：7 条横向彩色条形图，每条包含缩写标签、彩色进度条、百分比；当前最高情绪加白色边框高亮

**4.5 音乐面板**（右下角 242x164）：Key/Tempo/Velocity 参数 + 8 根竖条均衡器动画（播放时绿色正弦波跳动，空闲时灰色静态）

**4.6 底部提示栏**：居中显示 "ESC:Quit | SPACE:Play | M:Auto Toggle"

**4.7 人脸框**：绿色矩形改为四角装饰线（3px 粗），颜色随情绪变化

**4.8 关键点**：红色 radius=2 改为浅蓝色 radius=1，减少视觉干扰

#### 五、main.cpp 重构

将原来 ~80 行散落的 `putText`/`rectangle` 代码全部删除，替换为：
```cpp
OverlayRenderer renderer;
renderer.render(frame, fps, current_emotion, confidences, params,
                player.isPlaying(), music_enabled, face_detected, total_frames);
```

#### 六、修改文件汇总

| 文件 | 改动 |
|------|------|
| `src/ui/overlay_renderer.h` | 新建 |
| `src/ui/overlay_renderer.cpp` | 新建，~230 行 |
| `src/main.cpp` | 删除 ~80 行 UI 代码，替换为 renderer 调用 |
| `src/detector/face_detector.h` | `drawFace` 增加颜色参数 |
| `src/detector/face_detector.cpp` | 四角装饰线 + 浅蓝关键点 |
| `CMakeLists.txt` | SOURCES 添加 overlay_renderer.cpp |

---

#### 下一步计划

- [x] 在 VM 上测试 ONNX 推理准确率
- [x] 合并 dev/opencv-dnn 到 main
- [x] UI 优化（半透明面板 + 条形图 + 均衡器动画）
- [ ] 摄像头实时模式完整测试

---

## 日期：2026-04-26

### 任务：摄像头实时模式测试 + 音色优化

#### 一、摄像头模式首次完整测试

在 Ubuntu VM 上首次运行摄像头实时模式，逐一排查问题。

**问题1：模型路径错误**

```bash
cd build && ./emotion_music_generator camera
# [FaceDetector] 模型加载失败: Unable to open models/shape_predictor_68_face_landmarks.dat
```

原因：从 `build/` 目录运行，相对路径 `models/` 解析为 `build/models/`。

解决方案：从项目根目录运行：
```bash
./build/emotion_music_generator camera
```

**问题2：VirtualBox 摄像头透传**

VM 中 `ls /dev/video*` 找不到摄像头设备。

解决方案：VirtualBox 菜单 → 设备 → 摄像头 → 选择主机摄像头。

**问题3：Happy 表情被识别为 Sad**

摄像头模式下微笑被错误识别为 Sad。

**排查**：将 `SMOOTH_WINDOW` 从 5 改为 1（关闭平滑），确认是模型本身识别错误，不是平滑问题。

**根因**：`emotion_recognizer.cpp` 预处理缺少 BGR→RGB 颜色转换。OpenCV 摄像头帧为 BGR 格式，但 HSEmotion 模型训练时使用 RGB 输入。通道顺序错误导致 Blue/Red 互换，模型输出严重偏移。

**修复**：在 resize 后加一行：
```cpp
cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
```

修复后 Happy/Angry/Neutral 均识别正确。

#### 二、音乐音色优化

**问题**：原始音色为纯正弦波 + 简单包络，听起来像"AI 电子音"，不好听。

**优化1：基础音色改善（第一轮）**

| 改进项 | 之前 | 之后 |
|--------|------|------|
| 波形 | 纯正弦波 | 叠加4层泛音（基频+2/3/4次谐波） |
| 包络 | 简单 Attack+Release | 完整 ADSR |
| 节奏 | 所有音符相同时长 | 混合二分/四分/八分音符 |
| 旋律 | 纯随机游走 | 60%后倾向回归根音 + 结尾解决 |

**优化2：多音色乐器模拟（第二轮）**

根据情绪的 `mood` 字段，使用不同音色合成方法：

| 情绪 | mood | 音色 | 合成特征 |
|------|------|------|---------|
| Happy / Surprised | bright / playful | 明亮钢琴 | 微失谐双振荡器 + 非整数泛音 + 指数衰减 |
| Neutral | calm | 柔和钢琴 | 力度响应 + 钢琴衰减包络 |
| Sad | melancholic | 柔和弦乐 | 慢起音 + 颤音(vibrato) + 二次曲线包络 |
| Disgust | dark | 暗色铺底 | LFO 音量起伏 + 三次曲线包络 |
| Fear | tense | 拨弦/吉他 | 丰富高次谐波 + 快速衰减 |
| Angry | intense | 粗犷失真 | 锯齿波 + tanh 软削波 + 微噪声 |

**改动文件**：

| 文件 | 改动 |
|------|------|
| `src/audio/audio_player.h` | 新增 `Timbre` 枚举，`play()` 接受 mood 参数，新增 5 种合成方法声明 |
| `src/audio/audio_player.cpp` | 新增 `synthPiano/synthStrings/synthPad/synthPlucked/synthHarsh` 5 种合成方法，每种模拟不同乐器的物理特征 |
| `src/generator/music_generator.cpp` | 节奏变化 + 主音回归引力 + 结尾解决 |
| `src/main.cpp` | `player.play()` 调用传入 mood 参数 |
| `src/detector/emotion_recognizer.cpp` | BGR→RGB 颜色转换修复 |

#### 三、Git 网络配置调整

- 离开校园网后，原代理 `10.38.70.118:7897` 不通
- Windows 端改用本地代理 `127.0.0.1:7897`
- VM 端无法通过代理访问 GitHub（宿主机代理端口未对 VM 开放）
- VM 端 git pull 暂时无法使用，需配置"允许局域网连接"或手动同步

---

## 日期：2026-04-29

### 任务：音乐生成重写（初版）— 从随机音符到和弦进行+旋律+伴奏

在 VM 上完成和弦进行系统的初版实现，包括罗马数字解析引擎、基于和弦音的旋律生成、三种左手伴奏模式和柔和钢琴音色。VM 网络不通，代码通过 GitHub 间接同步。

```
d577caf feat: 和弦进行+旋律+伴奏重写
07cccb8 feat: 新增柔和钢琴音色(synthSoftPiano)
dc66294 docs: 更新开发日志(4/29)
```

---

## 日期：2026-05-01

### 任务：伴奏系统重写（优化版）— 简化架构 + 调优参数

#### 一、背景

4/29 初版使用罗马数字字符串（`"I-V-vi-IV"`）+ `ChordDef` 解析引擎 + `Composition` 结构体，架构较复杂。用户要求进一步调优伴奏。本次重写简化为整数度数数组方案，减少一层字符串解析。

#### 二、架构变化

| 对比项 | 4/29 初版 | 5/1 优化版 |
|--------|----------|-----------|
| 和弦进行表示 | 罗马数字字符串 `"I-V-vi-IV"` | 整数度数数组 `{0,4,5,3}` |
| 和弦解析 | `ChordDef` + `parseProgression()` | `buildChord()` 直接从音阶构建 |
| 数据结构 | `Composition{melody, accompaniment}` | `vector<TimedNote>` 扁平列表 |
| 休止符 | `Note.is_rest` 布尔字段 | 乐句间时间间隔 |
| 伴奏音色 | 独立柔和钢琴 | 与旋律共用音色 |
| 伴奏模式名 | `arpeggio` | `arpeggiated` |

#### 三、7 种情绪和弦进行配置（调优后）

| 情绪 | 调/音阶 | BPM | 和弦进行 | 伴奏模式 |
|------|---------|-----|---------|---------|
| Happy | C major | 140 | I-V-vi-IV（流行阳光） | 分解和弦 |
| Sad | A minor | 60 | i-VII-VI-VII（安达卢西亚） | 柱式和弦 |
| Angry | E minor | 160 | i-iv-V-i（强力小调） | 柱式和弦 |
| Surprised | G major | 120 | I-IV-V-I（明亮号角） | 分解和弦 |
| Neutral | C major | 90 | I-vi-IV-V（50 年代经典） | 阿尔贝蒂低音 |
| Fear | B minor | 100 | i-iv-V-i（紧张小调） | 分解和弦 |
| Disgust | F blues | 80 | i-VI-V-iv（暗沉下行） | 阿尔贝蒂低音 |

#### 四、修改文件汇总

| 文件 | 改动 | 行数变化 |
|------|------|---------|
| `src/mapper/emotion_mapper.h` | MusicParams 新增 3 字段 | +3 行 |
| `src/mapper/emotion_mapper.cpp` | 7 种情绪配置 | +8 行 |
| `src/generator/music_generator.h` | TimedNote + 新接口 | +17 行 |
| `src/generator/music_generator.cpp` | 完全重写 | +280 -109 行 |
| `src/audio/audio_player.h` | 新增 playComposition + synthSoftPiano | +3 行 |
| `src/audio/audio_player.cpp` | synthSoftPiano + playComposition | +96 -67 行 |
| `src/main.cpp` | API 切换 | +4 -4 行 |
| **总计** | **7 个文件** | **+398 -105 行** |

#### 五、待办

- [ ] VM 拉取本次更新（需配置 VM 网络或手动同步）
- [ ] VM 上编译测试
- [ ] 摄像头实时模式完整功能测试
- [ ] 根据试听效果进一步微调伴奏参数
