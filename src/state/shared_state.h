#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <mutex>
#include <vector>
#include <string>
#include <atomic>
#include <array>
#include <opencv2/opencv.hpp>
#include "detector/emotion_recognizer.h"
#include "mapper/emotion_mapper.h"
#include "profiler/perf_profiler.h"

// 主线程与 Web 服务线程之间的共享状态
struct SharedState {
    // ---- 最新帧（MJPEG 推流用）----
    std::mutex frame_mutex;
    cv::Mat latest_frame;

    void setFrame(const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(frame_mutex);
        latest_frame = frame.clone();
    }

    cv::Mat getFrame() {
        std::lock_guard<std::mutex> lock(frame_mutex);
        return latest_frame.clone();
    }

    // ---- 情绪数据（主线程写，Web 线程读）----
    std::mutex emotion_mutex;
    Emotion current_emotion = Emotion::NEUTRAL;
    std::vector<float> confidences;
    MusicParams current_params;
    bool face_detected = false;
    double fps = 0.0;
    int frame_count = 0;

    void setEmotionData(Emotion emo, const std::vector<float>& conf,
                        const MusicParams& params, bool face, double f, int fc) {
        std::lock_guard<std::mutex> lock(emotion_mutex);
        current_emotion = emo;
        confidences = conf;
        current_params = params;
        face_detected = face;
        fps = f;
        frame_count = fc;
    }

    // ---- 控制请求（Web 线程写，主线程读并清除）----
    std::atomic<bool> play_requested{false};
    std::atomic<bool> auto_toggle_requested{false};
    std::atomic<bool> mode_switch_requested{false};
    std::atomic<bool> pause_requested{false};

    // ---- 状态广播（主线程写，Web 线程读）----
    std::atomic<bool> music_enabled{true};
    std::atomic<bool> is_playing{false};
    std::atomic<int> playback_mode{0};  // 0 = 合成, 1 = 本地音乐
    bool web_enabled = false;
    int web_port = 8080;

    // ---- 性能探针数据（主线程写，Web/UI 线程读）----
    std::mutex perf_mutex;
    std::array<StageTiming, STAGE_COUNT> perf_timings{};
    double perf_total_ms = 0.0;  // 一帧管线总耗时

    void setPerfData(const std::array<StageTiming, STAGE_COUNT>& timings, double total_ms) {
        std::lock_guard<std::mutex> lock(perf_mutex);
        perf_timings = timings;
        perf_total_ms = total_ms;
    }

    void getPerfData(std::array<StageTiming, STAGE_COUNT>& timings, double& total_ms) {
        std::lock_guard<std::mutex> lock(perf_mutex);
        timings = perf_timings;
        total_ms = perf_total_ms;
    }
};

// 前向声明 + 全局指针，让 WebServer 可以访问 EmotionLogger
class EmotionLogger;
extern EmotionLogger* g_logger;

#endif // SHARED_STATE_H
