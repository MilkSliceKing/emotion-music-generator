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

优点：
- Commit Message 规范化（feat/fix/docs 等前缀）
- SourceTree 可视化操作降低 Git 门槛
- 配合 conventional-changelog 可自动生成 CHANGELOG
- Commitizen 交互式引导降低记忆负担

缺点：
- 工具链割裂（SourceTree 管 GUI，Commitizen 要回终端）
- 引入 Node.js 依赖（非前端项目显得过重）
- SourceTree 大仓库性能差、偶尔崩溃
- Commitizen 对简单修改流程过重
- 团队推广成本高

轻量替代方案：VS Code Git 插件 + Conventional Commits 插件 / Husky + commitlint

---

### Q2: 在 Windows 还是 VM 里开发？

**用户**：请你结合实验目录下的txt文档和devlog内容，开始这一步的开发。但是这个项目最后验收是要能在ubuntu也就是linux虚拟机上运行的，你觉得我是要在windows系统上做完然后镜像到linux里头还是在虚拟机里头做？给我方案。

**AI 建议**：直接在 Linux 虚拟机里开发，推荐使用 VS Code Remote-SSH。

分析要点：
- 编译器不同（MSVC vs GCC），C++ 行为有差异
- OpenCV/dlib/PortAudio 是系统级库，跨平台路径完全不同
- dlib 在 Windows 上编译困难，Ubuntu 上 apt install 即可
- 摄像头接口不同（V4L2 vs DirectShow）
- 验收在 VM 里跑，最后移植可能翻车

最终方案：Windows 上用 AI 生成代码 → GitHub 推送 → VM 里 clone 编译运行

---

### Q3: 准备项目基础文件

**用户**：可以啊，你帮我准备好基础文件，把文件放在 G:\大二\下\linux\实验目录下。VM能正常开机并且连接VSCode顺畅。

**AI 操作**：生成完整项目骨架（22个文件），包括：
- 构建配置（CMakeLists.txt 含 BLAS/LAPACK/PortAudio 条件编译）
- 5个模块的源码（detector/mapper/generator/audio）
- 主程序 main.cpp
- 文档框架（designed.md, perf.md, ai_usage.md）
- 配置文件（config.yaml, .gitignore, .gitattributes, build.sh）

---

### Q4: VS Code SSH 打开本地文件夹等于在 Windows 运行？

**用户**：可是这个程序最终要在ubuntu运行，我在ssh打开了这个文件夹不是等于我在windows本地运行了吗？

**AI 纠正**：VS Code Remote-SSH 是编辑 VM 上的文件，不是编辑 Windows 文件。代码必须在 VM 的硬盘上。正确流程：

```
Windows: git push → GitHub
VM: git clone → 编译 → 运行
VS Code Remote-SSH: 连接 VM 编辑 VM 上的文件
```

---

### Q5: Git push 连接超时

**用户**：
```
fatal: unable to access 'https://github.com/...': Failed to connect to github.com port 443
```

**AI**：配置 Git 代理：
```bash
git config --global http.proxy http://127.0.0.1:7897
git config --global https.proxy http://127.0.0.1:7897
```
代理端口查自用户确认：混合代理端口 7897

---

### Q6: Git push rejected (non-fast-forward)

**用户**：
```
! [rejected] main -> main (non-fast-forward)
```

**AI**：先 `git pull --rebase`，产生冲突后 `git checkout --ours` 保留本地版本，最终 `git push --force`。

---

### Q7: VM 里 git clone 失败

**用户**：
```
GnuTLS recv error (-54): Error in the pull function
```

**AI**：VM 里也要配代理，走 Windows 的网络：
```bash
export https_proxy=http://Windows局域网IP:7897
```
前提：Windows 代理软件开启"允许局域网连接"

---

### Q8: 编译链接错误 cblas_dgemm

**用户**：
```
/usr/bin/ld: undefined reference to symbol 'cblas_dgemm'
error adding symbols: DSO missing from command line
```

**AI**：dlib 依赖 BLAS/LAPACK，修改 CMakeLists.txt：
```cmake
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
target_link_libraries(... ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES})
```

---

### Q9: 摄像头无法识别

**用户**：
```
Cannot identify device '/dev/video0'
[错误] 无法打开摄像头
```

**AI**：VMware 未映射 USB 摄像头。操作：VMware 菜单 → VM → Removable Devices → 选择摄像头 → Connect

---

### Q10: 模型文件找不到

**用户**：程序运行后报 `Unable to open models/shape_predictor_68_face_landmarks.dat`

**AI**：从 `build/` 目录运行时相对路径解析错误。解决：从项目根目录运行 `./build/emotion_music_generator`

---

### 运行成功

最终程序在 Ubuntu VM 上成功运行，摄像头正常工作，面部检测和关键点提取功能正常。

---

## 总结

| 项目 | 详情 |
|------|------|
| 开发时间 | 2026-04-12（约3小时） |
| AI 工具 | Claude Code (GLM 5.1) |
| 遇到问题数 | 6 个 |
| 全部解决 | 是 |
| 当前状态 | 面部检测 + 关键点提取可用，完整流水线待测试 |
