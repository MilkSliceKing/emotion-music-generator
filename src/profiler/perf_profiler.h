#ifndef PERF_PROFILER_H
#define PERF_PROFILER_H

#include <chrono>
#include <array>
#include <string>
#include <cstring>

// 管线阶段枚举
enum class Stage : int {
    CAPTURE = 0,     // 帧采集
    DETECT,          // 人脸检测 (dlib HOG)
    LANDMARK,        // 68 关键点提取
    RECOGNIZE,       // 情绪推理 (ONNX)
    SMOOTH,          // 情绪平滑 + 映射
    MUSIC_GEN,       // 音乐生成 + 播放
    UI_RENDER,       // UI 渲染 (overlay)
    STATE_UPDATE,    // 共享状态更新
    STAGE_COUNT      // 总数，内部使用
};

constexpr int STAGE_COUNT = static_cast<int>(Stage::STAGE_COUNT);

// 阶段显示名
inline const char* stageName(Stage s) {
    switch (s) {
        case Stage::CAPTURE:      return "Capture";
        case Stage::DETECT:       return "Detect ";
        case Stage::LANDMARK:     return "Landmrk";
        case Stage::RECOGNIZE:    return "Infer  ";
        case Stage::SMOOTH:       return "Smooth ";
        case Stage::MUSIC_GEN:    return "Music  ";
        case Stage::UI_RENDER:    return "UI     ";
        case Stage::STATE_UPDATE: return "State  ";
        default:                  return "???    ";
    }
}

// 单阶段计时数据
struct StageTiming {
    double last_ms  = 0.0;   // 最近一次耗时 (ms)
    double avg_ms   = 0.0;   // 滑动平均耗时 (ms)
    double max_ms   = 0.0;   // 历史最大耗时 (ms)
    int    count    = 0;     // 累计采样次数
};

// 轻量单例探针 — header-only，零分配热路径
// 线程安全：仅主线程写入，其他线程可容忍微量陈旧数据
class PerfProfiler {
public:
    static PerfProfiler& instance() {
        static PerfProfiler inst;
        return inst;
    }

    // 记录一次阶段耗时
    void record(Stage stage, double ms) {
        int i = static_cast<int>(stage);
        if (i < 0 || i >= STAGE_COUNT) return;
        auto& t = timings_[i];
        t.last_ms = ms;
        t.count++;
        // 指数滑动平均 (alpha=0.1)，低成本平滑
        if (t.count == 1) {
            t.avg_ms = ms;
        } else {
            t.avg_ms = t.avg_ms * 0.9 + ms * 0.1;
        }
        if (ms > t.max_ms) t.max_ms = ms;
    }

    // 获取全部计时数据（供 UI / Web 读取）
    void snapshot(std::array<StageTiming, STAGE_COUNT>& out) const {
        std::memcpy(out.data(), timings_.data(), sizeof(timings_));
    }

    // 单阶段快速读取
    const StageTiming& get(Stage stage) const {
        return timings_[static_cast<int>(stage)];
    }

    // 重置最大值（便于观察当前负载峰值）
    void resetMax() {
        for (int i = 0; i < STAGE_COUNT; ++i) {
            timings_[i].max_ms = timings_[i].last_ms;
        }
    }

private:
    PerfProfiler() = default;
    std::array<StageTiming, STAGE_COUNT> timings_{};
};

// RAII 自动计时守卫 — 放在代码块开头，出作用域自动记录
class ScopedTimer {
public:
    ScopedTimer(Stage stage)
        : stage_(stage),
          start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start_).count();
        PerfProfiler::instance().record(stage_, ms);
    }

    // 禁止拷贝/移动
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    Stage stage_;
    std::chrono::high_resolution_clock::time_point start_;
};

#endif // PERF_PROFILER_H
