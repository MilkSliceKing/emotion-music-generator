# AI 对话记录

本文档记录了项目开发过程中与 AI (Claude Code / GLM) 的完整对话，保留原始问答格式。

---

## AI 工具使用记录

### 使用的 AI 工具

| 工具名称 | 用途 | 使用阶段 |
|---------|------|---------|
| Claude Code (GLM 5.1) | 代码生成、架构设计、调试、文档编写 | 全程 |

### AI 辅助的具体内容

#### 2026-04-11：面部检测模块

- **AI 辅助内容**：CMakeLists.txt 构建配置、FaceDetector 类结构设计
- **人工修改**：根据实际编译错误调整依赖路径
- **最终结果**：面部检测功能正常运行

#### 2026-04-12：完整流水线 + VM 环境部署

- **AI 辅助内容**：项目骨架代码生成（22 个文件）、情绪识别算法设计、音乐生成算法、CMake 配置、文档框架
- **人工修改**：VM 环境中实际编译测试后调整依赖和路径
- **遇到的问题**：Git 代理配置、dlib BLAS/LAPACK 链接错误、摄像头 USB 映射、模型路径问题

#### 2026-04-13：情绪识别模型攻关

- **AI 辅助内容**：FER+ ONNX 模型集成、4 种归一化方案排查、DeepFace 对照实验、Python 桥接方案设计
- **人工参与**：在 VM 上逐个测试不同方案，提供测试截图和输出日志
- **关键转折**：AI 建议 DeepFace 对照实验，确认 FER+ 模型固有偏置，最终采用 C++ + Python 桥接方案

#### 2026-04-15：音频播放调试

- **AI 辅助内容**：PortAudio ALSA 通道排查、VMware 声卡掉线诊断、paplay 方案替代
- **人工参与**：在 VM 上测试各种音频方案，确认 paplay 可正常播放

#### 2026-04-16：性能优化 — daemon 模式 + ONNX 替代

- **AI 辅助内容**：pipe+fork 常驻 daemon 设计、OpenCV DNN + ONNX 集成方案
- **人工修改**：确认 daemon 模式推理从 1-2s 降到 0.1-0.3s

#### 2026-04-21：FERPlus → HSEmotion 模型替换

- **AI 辅助内容**：模型调研（搜索 HSEmotion 论文和源码）、预处理重写（224x224 RGB + ImageNet 归一化）、onnxruntime C++ API 集成
- **人工参与**：VM 上测试验证，确认识别准确
- **AI 自主验证**：阅读 HSEmotion Python 源码确认预处理细节

#### 2026-04-21：UI 优化

- **AI 辅助内容**：毛玻璃面板方案设计、OverlayRenderer 类设计、所有 UI 绘制代码
- **人工修改**：确认布局和颜色方案

#### 2026-04-26：BGR→RGB 修复 + 多音色乐器模拟

- **AI 辅助内容**：颜色通道顺序 bug 定位、5 种乐器音色合成算法
- **人工参与**：摄像头模式测试，发现 Happy 被识别为 Sad

#### 2026-04-29：和弦进行 + 旋律 + 伴奏重写

- **AI 辅助内容**：和弦进行系统设计、旋律生成算法、三种伴奏模式、柔和钢琴音色
- **人工参与**：试听反馈，确认音乐听感改善

#### 2026-05-05：钢琴音色重写 + 情绪曲线图 + 屏幕按钮

- **AI 辅助内容**：双段衰减包络、三弦模拟、Schroeder 混响、情绪历史曲线图、屏幕按钮交互
- **人工参与**：UI 交互体验确认

#### 2026-05-09：表情识别测试工具

- **AI 辅助内容**：emotion_test.cpp 批量测试工具、CSV 输出 + 混淆矩阵生成
- **人工参与**：积累测试图片，分析混淆矩阵结果（Angry ↔ Disgust 互相混淆）

---

### AI 生成代码占比

| 模块 | AI生成比例 | 说明 |
|------|-----------|------|
| face_detector | ~80% | dlib API 使用由 AI 生成，调试由人工完成 |
| emotion_recognizer | ~85% | onnxruntime API 和预处理由 AI 生成，模型选择和测试由人工完成 |
| emotion_mapper | ~95% | 查表映射，AI 生成 |
| music_generator | ~90% | 和弦进行 + 旋律生成算法由 AI 设计 |
| audio_player | ~90% | 波形合成 + 音色模拟由 AI 生成 |
| overlay_renderer | ~90% | UI 绘制代码由 AI 生成，布局方案由人工确认 |
| main.cpp | ~85% | 主循环框架由 AI 生成，流程调整由人工完成 |
| CMakeLists.txt | ~90% | 构建配置 |
| 文档 | ~80% | 框架由 AI 生成，实际开发数据由人工填写 |

---

### 使用体会

- **优点**：AI 能快速生成项目骨架和 API 调用代码，显著加速开发。尤其在模型调研（HSEmotion）和音频合成（钢琴音色模拟）方面，AI 的领域知识帮助很大。
- **局限**：AI 生成的模型集成代码需要针对具体模型调整预处理参数，FER+ 模型的固有偏置问题花了大量时间排查。音频在 VM 上的播放方案需要实际环境验证。
- **经验**：AI 生成的代码必须人工在目标环境（Ubuntu VM）上测试验证，不能直接信任。"先跑通再优化"的策略适合 AI 辅助开发。

---

## 详细对话记录

### 会话 1：2026-04-12 — 项目骨架开发 + VM 环境部署

**Q1: 解释 SourceTree + Commitizen 的 GitHub 上传方法**

用户：请你跟我解释一下sourcetree和commitizen一起用的github上传方法的技术需求和优缺点

AI 回答摘要：技术需求 Git + SourceTree + Node.js + Commitizen，优点是 commit 规范化，缺点是工具链割裂。

**Q2: Windows 还是 VM 开发？**

用户：这个项目最后验收是要能在ubuntu上运行的，Windows做完然后镜像到linux里头还是在虚拟机里头做？

AI 建议：直接在 Linux VM 里开发。最终方案：Windows 上用 AI 生成代码 → GitHub 推送 → VM 里 clone 编译运行。

**Q3-Q10: 部署问题**

| 编号 | 问题 | 解决方案 |
|------|------|---------|
| Q3 | 准备项目基础文件 | AI 生成 22 个文件 |
| Q4 | VS Code SSH 打开本地文件夹≠在 VM 上运行 | 代码必须在 VM 硬盘上 |
| Q5 | Git push 连接超时 | 配置代理 127.0.0.1:7897 |
| Q6 | Git push rejected | pull --rebase + push --force |
| Q7 | VM 里 git clone 失败 | VM 里配代理走 Windows 网络 |
| Q8 | 编译链接错误 cblas_dgemm | CMakeLists.txt 添加 BLAS/LAPACK |
| Q9 | 摄像头无法识别 | VMware USB 映射 |
| Q10 | 模型文件找不到 | 从项目根目录运行 |

---

### 会话 2：2026-04-13 — 情绪识别模型攻关

**Q11: 表情识别不准**

用户：它的表情情绪识别不是很准，是不是需要模型去训练他吗？

AI 解释：当前是规则判断（阈值分类），建议换预训练深度学习模型。

**Q12: 换 FER+ ONNX 模型**

AI 操作：换用 FER+ ONNX 模型。结果：加载成功，但全输出 Neutral。

**Q13-Q18: 排查过程**

- Q13: 支持图片/视频输入模式（替代不稳定的摄像头）
- Q14: 多人/半张脸的处理策略
- Q15: 人脸检测和表情识别都不满意，要求修改
- Q16: SSD 模型 prototxt 下载失败
- Q17: FER+ 4 种归一化排查全 Neutral
- Q18: Caffe 模型下载 404

**Q19: DeepFace 对照实验（关键转折）**

AI 用 DeepFace Python 库测试同一张图，准确识别 angry 86%。证明照片和模型都没问题，是 FER+ ONNX 与 OpenCV DNN 不兼容。

**Q20: C++ + DeepFace 桥接方案**

AI 设计桥接：C++ 人脸检测 → 裁剪保存临时图 → popen 调用 Python DeepFace → 解析结果。

测试结果：成功识别为 Angry！用户反馈：可以了，效果挺好的。

---

### 会话 3：2026-04-15 — 音频播放调试

**Q23: 端到端没声音**

用户：情绪检测完生成对应的声音也没问题，但没办法根据表情发出声音。

AI 排查：PortAudio ALSA 通道未路由到 PulseAudio。加上 VMware 声卡掉线。

修复：切换为 paplay/aplay WAV 播放方案。

用户反馈：终于有声了！

---

### 会话 4：2026-04-21 — ONNX 推理版 VM 测试

**Q24-Q33: FERPlus → HSEmotion 模型替换**

1. VM 测试 FERPlus：全 Neutral（与 4/13 同样问题）
2. AI 调研找到 HSEmotion EfficientNet-B0 模型
3. 重写预处理（224x224 RGB + ImageNet 归一化）
4. OpenCV 4.5.4 不兼容 → 换 onnxruntime
5. VM 测试：识别准确！合并到 main

**Q35: UI 优化**

AI 方案：半透明面板 + 条形图 + 均衡器动画 + 四角人脸框。+339 -73 行。

---

### 会话 5：2026-04-26 — BGR→RGB 修复 + 音色优化

- 摄像头模式 Happy 被识别为 Sad → 缺少 BGR→RGB 转换
- 多音色乐器模拟：5 种情绪对应不同合成方法

---

### 会话 6：2026-04-29 — 和弦进行 + 旋律 + 伴奏重写

用户反馈音乐听起来不像钢琴曲。AI 重写音乐生成模块：和弦进行系统 + 基于和弦的旋律 + 左手伴奏 + 乐句结构。

---

### 会话 7：2026-05-05 — 钢琴音色重写 + UI 增强

- 双段衰减 + 三弦模拟 + Schroeder 混响的钢琴音色
- 情绪历史曲线图 + 屏幕按钮交互

---

### 会话 8：2026-05-09 — 表情识别测试工具

- emotion_test 批量测试工具：85% 准确率
- 发现 Angry ↔ Disgust 互相混淆，Surprise → Fear 误判

---

## 情绪识别方案演进史

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

## 音频播放方案演进史

```
PortAudio 直接播放 → ALSA 通道不通 PulseAudio → 无声音
    ↓
paplay/aplay 播放 WAV → 走 PulseAudio → ✓ 有声音
```

## UI 演进史

```
朴素 putText 堆叠（左上角纯文字）
    ↓
毛玻璃面板 + 条形图 + 均衡器动画 + 四角人脸框（OverlayRenderer 封装）
    ↓
+ 情绪历史曲线图 + 屏幕按钮交互 + 全面抗锯齿
```
