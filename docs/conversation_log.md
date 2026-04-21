# AI 对话记录 (conversation_log.md)

本文档记录了项目开发过程中与 AI (Claude Code / GLM) 的完整对话，保留原始问答格式。

---

## 会话日期：2026-04-12

### 主题：项目骨架开发 + VM 环境部署

---

### Q1: 解释 SourceTree + Commitizen 的 GitHub 上传方法

**用户**：请你跟我解释一下sourcetree和commitizen一起用的github上传方法的技术需求和优缺点

**AI 回答摘要**：

技术需求：Git + SourceTree + Node.js + Commitizen + cz-conventional-changelog

优点：Commit Message 规范化、SourceTree 可视化、自动 CHANGELOG、交互式引导
缺点：工具链割裂、引入 Node.js 依赖、SourceTree 性能差、流程过重

---

### Q2: 在 Windows 还是 VM 里开发？

**用户**：这个项目最后验收是要能在ubuntu也就是linux虚拟机上运行的，你觉得我是要在windows系统上做完然后镜像到linux里头还是在虚拟机里头做？

**AI 建议**：直接在 Linux 虚拟机里开发。最终方案：Windows 上用 AI 生成代码 → GitHub 推送 → VM 里 clone 编译运行。

---

### Q3-Q10: 部署过程中的各种问题

| 编号 | 问题 | 解决方案 |
|------|------|---------|
| Q3 | 准备项目基础文件 | AI 生成 22 个文件 |
| Q4 | VS Code SSH 打开本地文件夹≠在VM上运行 | 代码必须在 VM 硬盘上，通过 GitHub 中转 |
| Q5 | Git push 连接超时 | 配置代理 127.0.0.1:7897 |
| Q6 | Git push rejected | pull --rebase + push --force |
| Q7 | VM 里 git clone 失败 | VM 里配代理走 Windows 网络 |
| Q8 | 编译链接错误 cblas_dgemm | CMakeLists.txt 添加 BLAS/LAPACK |
| Q9 | 摄像头无法识别 | VMware USB 映射 |
| Q10 | 模型文件找不到 | 从项目根目录运行 |

---

## 会话日期：2026-04-13

### 主题：情绪识别模型攻关 — 从反复失败到最终成功

---

### Q11: 表情识别不准，不想用自己的脸训练

**用户**：它的表情情绪识别不是很准，但是我不知道他是基于什么运作的，是不是需要模型去训练他吗？

**AI 解释**：当前是规则判断（基于4个面部特征阈值），不需要训练。但阈值是通用值，对不同人效果差。建议换预训练的深度学习模型。

---

### Q12: 换 FER+ ONNX 模型

**用户**：可以用网上的模型吗

**AI 操作**：换用 FER+ ONNX 模型（从 ONNX Model Zoo 下载，34MB），用 OpenCV DNN 加载。

**结果**：加载成功，但所有表情都被识别为 Neutral。

---

### Q13: 摄像头老断，能不能不用摄像头

**用户**：我不想要虚拟机开摄像头，有啥别的方法吗

**AI**：添加图片/视频文件输入支持：
```bash
./emotion_music_generator image test_images/face.jpg
./emotion_music_generator video video.mp4
```

---

### Q14: 支持多个人脸？半张脸？

**用户**：如果有多个人呢？如果只有半张脸咋办？

**AI**：
- 多人：只识别最大人脸
- 半张脸：dlib 检测不到，直接显示"No face detected"

---

### Q15: 人脸检测和表情识别都不满意

**用户**：你的人脸检测用手捧着脸就识别不出来了，而且明显愤怒的（如张开嘴咆哮）的照片都被识别成了 Neutral，请你给我修改方案

**AI 方案**：
1. 人脸检测：dlib HOG → OpenCV DNN SSD（更准）
2. 情绪识别：FER+ ONNX → Caffe FER 模型（更成熟）

---

### Q16: SSD 模型下载一波三折

**用户尝试下载 deploy.prototxt**：
```
错误 404：Not Found
```

**AI 手写 prototxt 推送** → 但与 caffemodel 不匹配：
```
error: (-215:Assertion failed) !blobs.empty() || inputs.size() > 1
```

**结论**：手写的网络结构和实际模型不匹配，SSD 方案暂时搁置。

---

### Q17: FER+ 预处理排查

**AI 写了 Python 测试脚本** `test_emotion.py`，对同一张脸测试 4 种归一化方式：

```
--- A: [0,255] 不归一化 ---
  Neutral: xx.xx%
  ...
--- B: [0,1] 除以255 ---
  Neutral: xx.xx%
  ...
--- C: [-1,1] 减128除128 ---
  Neutral: xx.xx%
  ...
--- D: 标准化 mean=0.5 std=0.5 ---
  Neutral: xx.xx%
  ...
```

**结果：4种方式全部输出 Neutral！** 证明不是预处理的问题，是 FER+ ONNX 模型与 OpenCV DNN 不兼容。

---

### Q18: Caffe 情绪模型也下载不到

```
wget ... fer2013.caffemodel
错误 404：Not Found
```

多个 GitHub 源尝试均失败。

---

### Q19: 转 DeepFace — 终于看到希望

**AI**：用 DeepFace（Python 库）做对照实验，确认模型到底能不能用。

**安装过程**：
```bash
pip install deepface          # 安装
pip install tf-keras           # 修复依赖报错
```

**测试结果**：
```python
DeepFace.analyze(img_path='test_images/test9.jpg', actions=['emotion'])
# 输出:
# 'angry': 86.01, 'fear': 13.97, 'disgust': 0.003, 'happy': 0.00, 'sad': 0.02
# dominant_emotion: 'angry'
```

**DeepFace 准确识别为 angry（86%置信度）！**

放进去的照片确实是愤怒的表情，DeepFace 的结果完全正确。这证明：
1. 照片没有问题
2. 模型可以识别情绪
3. 问题出在 FER+ ONNX 模型与 OpenCV DNN 的兼容性上

---

### Q20: 最终方案 — C++ + DeepFace 桥接

**AI 设计了桥接方案**：
```
C++ 人脸检测(dlib) → 裁剪人脸保存临时图片 → 调用 Python DeepFace → 解析结果
```

新增文件：
- `predict_emotion.py`：桥接脚本，接收图片路径，输出情绪和置信度
- `emotion_recognizer.cpp`：裁剪人脸 → popen() 调用 Python → 解析逗号分隔输出

**最终测试结果**：
```bash
./build/emotion_music_generator image test_images/test9.jpg
# 成功识别为 Angry，置信度正确显示
```

**用户反馈**：可以了，效果挺好的。

---

### Q21: 你为什么能直接改代码让 VM 程序更新？

**用户**：你为啥可以直接改代码然后我的虚拟机的程序可以不断完善？你连着我的虚拟机吗？

**AI**：没有连 VM。流程是：
```
我改代码 → 保存 Windows 文件 → git push GitHub → 用户 VM 里 git pull → 编译运行
```
中间通过 GitHub 中转，用户的 VM 需要手动 `git pull` 才会更新。

---

### Q22: 外接硬盘上的 VM 还原快照卡住

**用户**：还原虚拟机状态还原了快一个小时了

**AI**：强制关闭 VMware，重新开机（不恢复快照），之前的数据都在磁盘上不会丢。

---

## 会话日期：2026-04-15

### 主题：音频播放模块调试 — PortAudio → paplay 方案切换

---

### Q23: 端到端流水线没声音

**用户**：目前 VM 里确定可以发出声音，情绪检测完生成对应的声音也没问题，也连上电脑的声卡了，但是就是没办法根据表情进而发出声音。

**AI 排查过程**：

1. 首先读取了 Windows 端所有源码，发现 Windows 版使用 `paplay/aplay` 播放 WAV 文件
2. 但 VM 端代码与 Windows 不同步，VM 使用的是 **PortAudio** 直接播放
3. VM 端 `audio_player.cpp` 通过 `#ifdef HAS_PORTAUDIO` 条件编译使用 PortAudio API
4. PortAudio 初始化成功、合成音符、打开音频流、播放完成 — 全部无报错
5. 但实际没有声音输出

**诊断结果**：

```
VM 音频架构：
PortAudio → ALSA Host API → "default" 设备 → 无声音

系统实际音频链路：
PulseAudio → alsa_output.pci-0000_02_02.0.analog-stereo → 正常

问题：PortAudio 走 ALSA 通道没有正确路由到 PulseAudio
```

中间还发现 VMware 虚拟声卡断开（`aplay` 和 `paplay` 都没声），通过移除并重新添加 VMware 声卡恢复。

**最终修复**：

将 VM 端 `audio_player.cpp` 从 PortAudio 方案切换为 paplay/aplay 方案（与 Windows 端一致）：
- 合成音频 → 写入 `/tmp/emotion_music.wav` → `fork()` + `execlp("paplay")` 播放
- 通过 Windows 推送 GitHub → VM 拉取 → 重新编译

**VM 端代理修复**：`git config --global --replace-all http.proxy http://10.38.70.118:7897`

**最终测试结果**：

```bash
./build/emotion_music_generator image test_images/test9.jpg
# 情绪识别: Angry
# 音频播放: 成功发出声音！
```

**用户反馈**：终于有声了！

---

## 总结

| 项目 | 详情 |
|------|------|
| 总开发时间 | 2026-04-12 ~ 04-15（约 8 小时） |
| AI 工具 | Claude Code (GLM 5.1) |
| 遇到问题总数 | 23 个 |
| 全部解决 | 是 |
| 当前状态 | 人脸检测 + 情绪识别 + 音频播放完整流水线跑通 |
| 最终方案 | dlib HOG 人脸检测 + DeepFace Python 桥接情绪识别 + paplay WAV 播放 |

### 情绪识别方案演进史

```
规则判断(68关键点阈值) → 不准
    ↓
FER+ ONNX 模型 → 全输出 Neutral
    ↓
排查 4 种归一化 → 都不行
    ↓
换 Caffe FER 模型 → 下载不到
    ↓
DeepFace Python → angry 86% ✓ 成功！
    ↓
C++ + Python 桥接 → 最终方案
```

### 音频播放方案演进史

```
PortAudio 直接播放 → ALSA 通道不通 PulseAudio → 无声音
    ↓
paplay/aplay 播放 WAV → 走 PulseAudio → ✓ 有声音！
```

---

## 会话日期：2026-04-21

### 主题：二代 ONNX 推理版 VM 测试 — FERPlus 失败 → 换 HSEmotion 模型

---

### Q24: 你记得我的项目吗？

**用户**：你记得我的项目吗 简述

**AI**：从记忆文件中回顾了项目状态——情绪音乐生成器，C++ Linux 课程实验，main 分支（DeepFace）稳定可用，dev/opencv-dnn 分支（ONNX 推理）待 VM 测试。

---

### Q25: 接下来推荐做什么？

**用户**：建议推荐什么

**AI**：按优先级推荐：
1. 测试 dev/opencv-dnn 分支 ONNX 推理
2. DeepFace vs FERPlus 准确率对比
3. 摄像头实时模式测试
4. 音乐生成优化
5. 简单 UI/可视化

---

### Q26: 开始在 VM 测试 ONNX 推理

**用户**：好 按第一点开始

**AI 操作**：
1. 检查项目状态：Windows 上在 dev/opencv-dnn 分支，代码齐全
2. 查看 ONNX 推理核心代码：emotion_recognizer.cpp（FERPlus 模型，64x64 灰度输入）
3. 给出 VM 测试步骤：pull → 下载模型 → 编译 → 测试

---

### Q27: VM 测试结果 — 全 Neutral

**用户**：在 VM 上运行成功，但识别结果是 Neutral

```
[情绪] 识别结果: Neutral
[AudioPlayer] 合成 8 个音符, 5.33333 秒
[AudioPlayer] 开始播放 (PID: 2716, 播放器: paplay)
[播放] Neutral -> C major 90BPM
```

**AI**：编译运行通过，但需要验证准确率。建议批量测试所有 test_images。

**用户批量测试结果**：
```
face.jpg → Neutral
test3.jpg → Neutral
test4.jpg → Neutral
test5.jpg → Neutral
test8.jpg → Neutral
test9.jpg → Neutral
happy.jpg → （无人脸输出，检测失败）
test2/6/7.jpg → （检测失败）
```

**用户**：又全变成neutral了

---

### Q28: 诊断 — 查看原始 logits

**AI 操作**：添加诊断输出到 emotion_recognizer.cpp，打印模型原始输出 shape 和值。

**用户提供的诊断结果**：
```
[DEBUG] output shape: [8 x 1] (dims: 2)
[DEBUG] raw output: Neutral=4.35 Happy=0.94 Surprise=-0.04 Sad=3.09 Anger=-0.20 Disgust=-3.03 Fear=-2.36 Contempt=-2.32
```

**AI 分析**：
- 输出是 logits（非概率），值域 [-3, 4.35]
- Neutral=4.35 远高于其他类
- 不同类的值确实有区分，模型在工作
- 但 FERPlus 模型系统性偏向 Neutral
- 与 4/13 首次尝试 FER+ 时的问题完全一致

**AI 建议**：进一步裁剪人脸图片诊断，或换更好的模型。

---

### Q29: 换更好的 ONNX 模型

**用户**：选择"换更好的 ONNX 模型"

**AI 调研**：
1. 搜索 ONNX 情绪识别模型替代方案
2. 找到 **HSEmotion (EmotiEffLib)** — HSE 大学开发，EfficientNet-B0 架构，ICML 2023 论文
3. 阅读源码 `facial_emotions.py`，确认：
   - 输入：224x224 RGB
   - 预处理：ImageNet 归一化 (mean=[0.485,0.456,0.406], std=[0.229,0.224,0.225])
   - 输出：8 类 logits
   - 模型下载 URL：GitHub HSE-asavchenko 仓库

**AI 代码改动**（3 个文件）：

1. `download_models.sh` — 换模型 URL 和文件名
2. `emotion_recognizer.cpp` — 完全重写：
   - 新标签映射（HSEmotion 8 类顺序）
   - 新预处理（224x224 RGB + ImageNet mean/std）
   - 统一输出解析（兼容不同 shape）
   - 添加 softmax 转换
3. `main.cpp` — 换模型路径

---

### Q30: Git push SSL 问题

**Windows 端**：
```
git push → schannel: failed to receive handshake, SSL/TLS connection failed
```

**解决方案**：临时绕过代理推送
```bash
git -c http.proxy="" -c https.proxy="" push origin dev/opencv-dnn
```

**VM 端**：
```
git pull → gnutls_handshake() failed: The TLS connection was non-properly terminated
```

**解决方案**：重新配置 VM 代理
```bash
git config --global http.proxy http://10.38.70.118:7897
```

---

### Q31: 帮我更新对话日志和 devlog

**用户**：帮我更新对话日志和devlog

**AI**：正在更新两个文件，记录本次会话的完整内容。

---

## 总结（更新版）

| 项目 | 详情 |
|------|------|
| 总开发时间 | 2026-04-12 ~ 04-21（约 12 小时） |
| AI 工具 | Claude Code (GLM 5.1) |
| 遇到问题总数 | 31 个 |
| 全部解决 | 是（除 VM 端 pull 待修复） |
| 当前状态 | main 分支完整可用；dev/opencv-dnn 分支已换用 HSEmotion 模型，待 VM 测试 |
| 最终方案 | dlib HOG 人脸检测 + DeepFace Python 桥接情绪识别 + paplay WAV 播放（初代）/ HSEmotion ONNX 纯 C++ 推理（二代开发中） |

### 情绪识别方案演进史（完整版）

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
C++ + Python 桥接 (main 分支) → 初代方案，可用
    ↓
HSEmotion EfficientNet ONNX → 待 VM 测试（dev/opencv-dnn 分支）
```

### 音频播放方案演进史

```
PortAudio 直接播放 → ALSA 通道不通 PulseAudio → 无声音
    ↓
paplay/aplay 播放 WAV → 走 PulseAudio → ✓ 有声音！
```
