#include "overlay_renderer.h"
#include <cmath>
#include <algorithm>

// 情绪颜色 (BGR)
const cv::Scalar OverlayRenderer::EMOTION_COLORS[7] = {
    {0, 0, 255},      // ANGRY    = 红
    {0, 140, 140},    // DISGUST  = 暗青
    {180, 100, 0},    // FEAR     = 紫
    {0, 230, 255},    // HAPPY    = 黄
    {255, 150, 0},    // SAD      = 蓝
    {0, 200, 255},    // SURPRISED= 橙
    {200, 200, 200}   // NEUTRAL  = 灰白
};

const std::string OverlayRenderer::EMOTION_SHORT_NAMES[7] = {
    "Angry", "Disg", "Fear", "Happy", "Sad", "Surp", "Neut"
};

cv::Scalar OverlayRenderer::getEmotionColor(Emotion e) {
    int idx = static_cast<int>(e);
    if (idx >= 0 && idx < 7) return EMOTION_COLORS[idx];
    return cv::Scalar(200, 200, 200);
}

// ---- 半透明矩形 ----

void OverlayRenderer::drawTransRect(cv::Mat& frame, const cv::Rect& roi,
                                     const cv::Scalar& color, double alpha) {
    cv::Rect clamped = roi & cv::Rect(0, 0, frame.cols, frame.rows);
    if (clamped.area() <= 0) return;

    cv::Mat overlay = frame(clamped).clone();
    cv::rectangle(overlay, cv::Point(0, 0), cv::Point(clamped.width, clamped.height),
                  color, -1);
    cv::addWeighted(overlay, alpha, frame(clamped), 1.0 - alpha, 0, frame(clamped));
}

// ---- 顶部状态栏 ----

void OverlayRenderer::drawHeaderBar(cv::Mat& frame, double fps, Emotion emotion,
                                     bool is_playing, bool music_enabled) {
    drawTransRect(frame, cv::Rect(0, 0, frame.cols, 55), cv::Scalar(20, 20, 20), 0.7);

    // FPS
    std::string fps_text = "FPS: " + std::to_string(static_cast<int>(fps));
    cv::putText(frame, fps_text, cv::Point(12, 22),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

    // 播放状态指示器
    int indicator_x = 120;
    if (is_playing) {
        cv::circle(frame, cv::Point(indicator_x, 18), 5, cv::Scalar(0, 220, 0), -1);
        cv::putText(frame, "PLAYING", cv::Point(indicator_x + 10, 23),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 220, 0), 1);
    } else {
        cv::circle(frame, cv::Point(indicator_x, 18), 5, cv::Scalar(120, 120, 120), 1);
        cv::putText(frame, "IDLE", cv::Point(indicator_x + 10, 23),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(120, 120, 120), 1);
    }

    // 自动播放开关
    std::string auto_text = music_enabled ? "AUTO:ON" : "AUTO:OFF";
    cv::Scalar auto_color = music_enabled ? cv::Scalar(0, 200, 0) : cv::Scalar(0, 0, 200);
    cv::putText(frame, auto_text, cv::Point(frame.cols - 90, 22),
                cv::FONT_HERSHEY_SIMPLEX, 0.45, auto_color, 1);

    // 情绪标签（使用情绪对应颜色）
    std::string emo_text = "Emotion: " + emotionToString(emotion);
    cv::putText(frame, emo_text, cv::Point(12, 46),
                cv::FONT_HERSHEY_SIMPLEX, 0.65, getEmotionColor(emotion), 2);
}

// ---- 置信度条形图面板 ----

void OverlayRenderer::drawConfidencePanel(cv::Mat& frame,
                                           const std::vector<float>& confidences,
                                           int current_idx) {
    int panel_x = 0, panel_y = 298, panel_w = 250, panel_h = 158;
    drawTransRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h),
                  cv::Scalar(30, 30, 30), 0.65);

    if (confidences.size() < 7) return;

    int bar_y_start = panel_y + 4;
    int bar_h = 16;
    int bar_spacing = 22;
    int label_x = panel_x + 6;
    int bar_x = panel_x + 56;
    int bar_max_w = 158;
    int pct_x = panel_x + 220;

    for (int i = 0; i < 7; ++i) {
        int y = bar_y_start + i * bar_spacing;
        int text_y = y + bar_h - 2;

        // 标签
        cv::putText(frame, EMOTION_SHORT_NAMES[i], cv::Point(label_x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.38, cv::Scalar(200, 200, 200), 1);

        // 条形背景
        cv::rectangle(frame, cv::Point(bar_x, y), cv::Point(bar_x + bar_max_w, y + bar_h),
                      cv::Scalar(60, 60, 60), -1);

        // 条形前景
        int bar_w = static_cast<int>(confidences[i] * bar_max_w);
        if (bar_w > 0) {
            cv::rectangle(frame, cv::Point(bar_x, y),
                          cv::Point(bar_x + bar_w, y + bar_h),
                          EMOTION_COLORS[i], -1);
        }

        // 当前最高情绪加白色边框
        if (i == current_idx) {
            cv::rectangle(frame, cv::Point(bar_x - 1, y - 1),
                          cv::Point(bar_x + bar_max_w + 1, y + bar_h + 1),
                          cv::Scalar(255, 255, 255), 2);
        }

        // 百分比
        int pct = static_cast<int>(confidences[i] * 100);
        std::string pct_text = std::to_string(pct) + "%";
        cv::putText(frame, pct_text, cv::Point(pct_x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(200, 200, 200), 1);
    }
}

// ---- 音乐参数面板 ----

void OverlayRenderer::drawMusicPanel(cv::Mat& frame, const MusicParams& params,
                                      bool is_playing, int frame_count) {
    int panel_x = 398, panel_y = 298, panel_w = 242, panel_h = 158;
    drawTransRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h),
                  cv::Scalar(30, 30, 30), 0.65);

    // 标题
    cv::putText(frame, "MUSIC", cv::Point(panel_x + 12, panel_y + 22),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(180, 180, 180), 1);

    // 调性
    std::string key_text = "Key: " + params.key + " " + params.scale;
    cv::putText(frame, key_text, cv::Point(panel_x + 12, panel_y + 47),
                cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(220, 220, 220), 1);

    // 节拍
    std::string tempo_text = "Tempo: " + std::to_string(params.tempo) + " BPM";
    cv::putText(frame, tempo_text, cv::Point(panel_x + 12, panel_y + 72),
                cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(220, 220, 220), 1);

    // 力度
    std::string vel_text = "Velocity: " + std::to_string(params.velocity);
    cv::putText(frame, vel_text, cv::Point(panel_x + 12, panel_y + 97),
                cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(180, 180, 180), 1);

    // 均衡器动画
    int eq_y_base = panel_y + 148;
    int bar_w = 8, bar_gap = 14;
    int eq_x_start = panel_x + 18;

    for (int b = 0; b < 8; ++b) {
        int bar_h = is_playing
            ? 6 + static_cast<int>(10 * std::abs(std::sin(frame_count * 0.25 + b * 0.9)))
            : 4;
        int bx = eq_x_start + b * bar_gap;
        int by = eq_y_base - bar_h;
        cv::Scalar bar_color = is_playing
            ? cv::Scalar(0, 220, 100) : cv::Scalar(80, 80, 80);
        cv::rectangle(frame, cv::Point(bx, by), cv::Point(bx + bar_w, eq_y_base),
                      bar_color, -1);
    }

    // 播放状态文字
    std::string status = is_playing ? "PLAYING" : "IDLE";
    cv::Scalar status_color = is_playing ? cv::Scalar(0, 220, 100) : cv::Scalar(100, 100, 100);
    cv::putText(frame, status, cv::Point(eq_x_start + 8 * bar_gap + 8, eq_y_base - 4),
                cv::FONT_HERSHEY_SIMPLEX, 0.35, status_color, 1);
}

// ---- 底部提示栏 ----

void OverlayRenderer::drawFooterBar(cv::Mat& frame) {
    drawTransRect(frame, cv::Rect(0, frame.rows - 24, frame.cols, 24),
                  cv::Scalar(20, 20, 20), 0.7);

    std::string help_text = "ESC: Quit  |  SPACE: Play  |  M: Auto Toggle";
    int text_w = cv::getTextSize(help_text, cv::FONT_HERSHEY_SIMPLEX, 0.38, 1, nullptr).width;
    cv::putText(frame, help_text,
                cv::Point((frame.cols - text_w) / 2, frame.rows - 7),
                cv::FONT_HERSHEY_SIMPLEX, 0.38, cv::Scalar(170, 170, 170), 1);
}

// ---- 无脸提示 ----

void OverlayRenderer::drawNoFaceHint(cv::Mat& frame) {
    std::string hint = "No face detected";
    int text_w = cv::getTextSize(hint, cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, nullptr).width;
    int cx = (frame.cols - text_w) / 2;
    int cy = frame.rows / 2;

    // 半透明背景
    drawTransRect(frame, cv::Rect(cx - 15, cy - 25, text_w + 30, 50),
                  cv::Scalar(20, 20, 20), 0.7);

    // 简化头像图标
    int icon_x = cx - 5;
    cv::circle(frame, cv::Point(icon_x - 40, cy - 5), 8, cv::Scalar(0, 0, 255), 2);
    cv::line(frame, cv::Point(icon_x - 40, cy + 3), cv::Point(icon_x - 40, cy + 18),
             cv::Scalar(0, 0, 255), 2);

    // 文字
    cv::putText(frame, hint, cv::Point(cx, cy),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);
}

// ---- 总渲染入口 ----

void OverlayRenderer::render(cv::Mat& frame,
                              double fps,
                              Emotion current_emotion,
                              const std::vector<float>& confidences,
                              const MusicParams& music_params,
                              bool is_playing,
                              bool music_enabled,
                              bool face_detected,
                              int frame_count) {
    // 1. 顶部状态栏
    drawHeaderBar(frame, fps, current_emotion, is_playing, music_enabled);

    // 2. 置信度面板（仅检测到人脸时）
    if (face_detected && confidences.size() >= 7) {
        drawConfidencePanel(frame, confidences, static_cast<int>(current_emotion));
    }

    // 3. 音乐面板
    drawMusicPanel(frame, music_params, is_playing, frame_count);

    // 4. 底部提示栏
    drawFooterBar(frame);

    // 5. 无脸提示（最后绘制，覆盖在其他元素之上）
    if (!face_detected) {
        drawNoFaceHint(frame);
    }
}
