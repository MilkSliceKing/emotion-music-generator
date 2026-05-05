#include "overlay_renderer.h"
#include <cmath>
#include <algorithm>

// 情绪颜色 (BGR) — 更鲜艳现代的配色
const cv::Scalar OverlayRenderer::EMOTION_COLORS[7] = {
    {60, 60, 255},      // ANGRY    = 亮红
    {180, 120, 0},      // DISGUST  = 琥珀
    {200, 80, 180},     // FEAR     = 紫粉
    {60, 220, 255},     // HAPPY    = 明黄
    {220, 130, 30},     // SAD      = 钴蓝
    {50, 180, 255},     // SURPRISED= 橘色
    {220, 220, 220}     // NEUTRAL  = 浅灰
};

const std::string OverlayRenderer::EMOTION_SHORT_NAMES[7] = {
    "Angry", "Disg", "Fear", "Happy", "Sad", "Surp", "Neut"
};

cv::Scalar OverlayRenderer::getEmotionColor(Emotion e) {
    int idx = static_cast<int>(e);
    if (idx >= 0 && idx < 7) return EMOTION_COLORS[idx];
    return cv::Scalar(200, 200, 200);
}

// ========== 基础绘制工具 ==========

void OverlayRenderer::drawFrostedRect(cv::Mat& frame, const cv::Rect& roi,
                                        const cv::Scalar& tint_color,
                                        double alpha, int blur_size) {
    cv::Rect clamped = roi & cv::Rect(0, 0, frame.cols, frame.rows);
    if (clamped.area() <= 0) return;

    // 扩大模糊范围避免边缘问题
    cv::Rect blur_roi = clamped;
    int margin = blur_size / 2 + 1;
    blur_roi.x = std::max(0, blur_roi.x - margin);
    blur_roi.y = std::max(0, blur_roi.y - margin);
    blur_roi.width = std::min(frame.cols - blur_roi.x, clamped.width + 2 * margin);
    blur_roi.height = std::min(frame.rows - blur_roi.y, clamped.height + 2 * margin);

    // 在副本上做模糊
    cv::Mat blurred;
    cv::GaussianBlur(frame(blur_roi), blurred, cv::Size(blur_size, blur_size), 0);

    // 裁剪到目标区域
    cv::Rect inner(clamped.x - blur_roi.x, clamped.y - blur_roi.y, clamped.width, clamped.height);
    cv::Mat blurred_region = blurred(inner);

    // 叠加半透明色块
    cv::Mat tinted = blurred_region.clone();
    cv::rectangle(tinted, cv::Point(0, 0), cv::Point(clamped.width, clamped.height),
                  tint_color, -1);
    cv::addWeighted(tinted, alpha, blurred_region, 1.0 - alpha, 0, frame(clamped));
}

void OverlayRenderer::drawRoundRect(cv::Mat& frame, const cv::Rect& rect, int radius,
                                      const cv::Scalar& color, int thickness) {
    int r = std::min({radius, rect.width / 2, rect.height / 2});
    if (r < 1) r = 1;
    cv::Point tl(rect.x + r, rect.y + r);
    cv::Point br(rect.x + rect.width - r, rect.y + rect.height - r);

    if (thickness == -1) {
        // 填充模式
        cv::ellipse(frame, tl, cv::Size(r, r), 180, 0, 90, color, -1);
        cv::ellipse(frame, cv::Point(br.x, tl.y), cv::Size(r, r), 270, 0, 90, color, -1);
        cv::ellipse(frame, br, cv::Size(r, r), 0, 0, 90, color, -1);
        cv::ellipse(frame, cv::Point(tl.x, br.y), cv::Size(r, r), 90, 0, 90, color, -1);
        cv::rectangle(frame, cv::Point(tl.x, rect.y), cv::Point(br.x, rect.y + rect.height), color, -1);
        cv::rectangle(frame, cv::Point(rect.x, tl.y), cv::Point(rect.x + rect.width, br.y), color, -1);
    } else {
        // 描边模式
        cv::ellipse(frame, tl, cv::Size(r, r), 180, 0, 90, color, thickness);
        cv::ellipse(frame, cv::Point(br.x, tl.y), cv::Size(r, r), 270, 0, 90, color, thickness);
        cv::ellipse(frame, br, cv::Size(r, r), 0, 0, 90, color, thickness);
        cv::ellipse(frame, cv::Point(tl.x, br.y), cv::Size(r, r), 90, 0, 90, color, thickness);
        cv::line(frame, cv::Point(tl.x, rect.y), cv::Point(br.x, rect.y), color, thickness);
        cv::line(frame, cv::Point(tl.x, rect.y + rect.height), cv::Point(br.x, rect.y + rect.height), color, thickness);
        cv::line(frame, cv::Point(rect.x, tl.y), cv::Point(rect.x, br.y), color, thickness);
        cv::line(frame, cv::Point(rect.x + rect.width, tl.y), cv::Point(rect.x + rect.width, br.y), color, thickness);
    }
}

void OverlayRenderer::drawGlowText(cv::Mat& frame, const std::string& text, cv::Point org,
                                     int font_face, double font_scale, const cv::Scalar& color,
                                     int thickness, int glow_layers) {
    // 从外到内画多层半透明文字模拟光晕
    for (int i = glow_layers; i >= 1; --i) {
        double alpha = 0.15 / i;
        cv::Scalar glow_color(
            static_cast<double>(color[0]) * alpha + 128 * (1 - alpha),
            static_cast<double>(color[1]) * alpha + 128 * (1 - alpha),
            static_cast<double>(color[2]) * alpha + 128 * (1 - alpha)
        );
        cv::putText(frame, text, org, font_face, font_scale, glow_color, thickness + i * 2);
    }
    // 最终清晰文字
    cv::putText(frame, text, org, font_face, font_scale, color, thickness);
}

// ========== 顶部状态栏 ==========

void OverlayRenderer::drawHeaderBar(cv::Mat& frame, double fps, Emotion emotion,
                                      bool is_playing, bool music_enabled) {
    int bar_h = 60;

    // 毛玻璃背景
    drawFrostedRect(frame, cv::Rect(0, 0, frame.cols, bar_h),
                    cv::Scalar(15, 15, 20), 0.45, 25);

    // 细线分隔
    cv::line(frame, cv::Point(0, bar_h - 1), cv::Point(frame.cols, bar_h - 1),
             cv::Scalar(80, 80, 100, 100), 1);

    // FPS — 小号淡色
    std::string fps_text = std::to_string(static_cast<int>(fps)) + " fps";
    cv::putText(frame, fps_text, cv::Point(16, 26),
                cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(120, 120, 130), 1);

    // 播放状态 — 胶囊形状
    int pill_x = 90, pill_y = 10, pill_w = 70, pill_h = 20, pill_r = 10;
    cv::Scalar pill_color = is_playing ? cv::Scalar(20, 160, 80) : cv::Scalar(60, 60, 70);
    drawRoundRect(frame, cv::Rect(pill_x, pill_y, pill_w, pill_h), pill_r, pill_color, -1);

    // 播放/暂停图标
    if (is_playing) {
        // 两个小竖条（暂停图标）
        cv::rectangle(frame, cv::Point(pill_x + 10, pill_y + 5),
                      cv::Point(pill_x + 14, pill_y + 15), cv::Scalar(255, 255, 255), -1);
        cv::rectangle(frame, cv::Point(pill_x + 18, pill_y + 5),
                      cv::Point(pill_x + 22, pill_y + 15), cv::Scalar(255, 255, 255), -1);
    } else {
        // 三角形（播放图标）
        cv::Point pts[3] = {
            cv::Point(pill_x + 11, pill_y + 5),
            cv::Point(pill_x + 11, pill_y + 15),
            cv::Point(pill_x + 23, pill_y + 10)
        };
        cv::fillConvexPoly(frame, pts, 3, cv::Scalar(255, 255, 255));
    }
    cv::putText(frame, is_playing ? "PLAY" : "IDLE",
                cv::Point(pill_x + 28, pill_y + 15),
                cv::FONT_HERSHEY_SIMPLEX, 0.32, cv::Scalar(240, 240, 240), 1);

    // 自动播放开关 — 右侧
    int auto_x = frame.cols - 100, auto_y = 10;
    std::string auto_text = music_enabled ? "AUTO" : "AUTO";
    cv::Scalar auto_bg = music_enabled ? cv::Scalar(20, 160, 80) : cv::Scalar(60, 60, 70);
    drawRoundRect(frame, cv::Rect(auto_x, auto_y, 85, 20), 10, auto_bg, -1);

    // 开关圆点
    int dot_x = music_enabled ? auto_x + 67 : auto_x + 18;
    cv::circle(frame, cv::Point(dot_x, auto_y + 10), 7, cv::Scalar(255, 255, 255), -1);
    cv::putText(frame, music_enabled ? "ON" : "OFF",
                cv::Point(music_enabled ? auto_x + 12 : auto_x + 40, auto_y + 15),
                cv::FONT_HERSHEY_SIMPLEX, 0.32, cv::Scalar(255, 255, 255), 1);

    // 情绪标签 — 大号发光文字
    std::string emo_text = emotionToString(emotion);
    cv::Scalar emo_color = getEmotionColor(emotion);
    drawGlowText(frame, emo_text, cv::Point(16, 52),
                 cv::FONT_HERSHEY_SIMPLEX, 0.75, emo_color, 2, 3);

    // 情绪色小圆点
    cv::circle(frame, cv::Point(frame.cols - 20, 30), 8, emo_color, -1);
    cv::circle(frame, cv::Point(frame.cols - 20, 30), 10, emo_color, 2);
}

// ========== 置信度面板 ==========

void OverlayRenderer::drawConfidencePanel(cv::Mat& frame,
                                            const std::vector<float>& confidences,
                                            int current_idx) {
    int panel_x = 10, panel_y = 275, panel_w = 240, panel_h = 195;
    int panel_r = 12;

    // 毛玻璃圆角面板
    drawFrostedRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h),
                    cv::Scalar(15, 15, 20), 0.4, 21);
    // 圆角边框
    drawRoundRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h), panel_r,
                  cv::Scalar(60, 60, 80), 1);

    if (confidences.size() < 7) return;

    // 标题
    cv::putText(frame, "CONFIDENCE", cv::Point(panel_x + 14, panel_y + 18),
                cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(140, 140, 160), 1);

    int bar_y_start = panel_y + 28;
    int bar_h = 14;
    int bar_spacing = 22;
    int label_x = panel_x + 10;
    int bar_x = panel_x + 58;
    int bar_max_w = 140;
    int pct_x = panel_x + 204;

    for (int i = 0; i < 7; ++i) {
        int y = bar_y_start + i * bar_spacing;
        int text_y = y + bar_h - 2;
        bool is_top = (i == current_idx);

        // 标签
        cv::Scalar label_color = is_top ? cv::Scalar(230, 230, 240) : cv::Scalar(130, 130, 140);
        cv::putText(frame, EMOTION_SHORT_NAMES[i], cv::Point(label_x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.33, label_color, 1);

        // 条形背景（圆角感）
        cv::rectangle(frame, cv::Point(bar_x, y + 2), cv::Point(bar_x + bar_max_w, y + bar_h - 2),
                      cv::Scalar(40, 40, 50), -1);

        // 条形前景
        int bar_w = static_cast<int>(confidences[i] * bar_max_w);
        if (bar_w > 3) {
            cv::Scalar bar_color = EMOTION_COLORS[i];
            // 顶部情绪的条更亮
            if (is_top) {
                bar_color = cv::Scalar(
                    std::min(255.0, bar_color[0] * 1.3),
                    std::min(255.0, bar_color[1] * 1.3),
                    std::min(255.0, bar_color[2] * 1.3)
                );
            }
            cv::rectangle(frame, cv::Point(bar_x, y + 2),
                          cv::Point(bar_x + bar_w, y + bar_h - 2),
                          bar_color, -1);
        }

        // 百分比
        int pct = static_cast<int>(confidences[i] * 100);
        std::string pct_text = std::to_string(pct) + "%";
        cv::Scalar pct_color = is_top ? cv::Scalar(255, 255, 255) : cv::Scalar(150, 150, 160);
        cv::putText(frame, pct_text, cv::Point(pct_x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.35, pct_color, is_top ? 2 : 1);
    }
}

// ========== 音乐面板 ==========

void OverlayRenderer::drawMusicPanel(cv::Mat& frame, const MusicParams& params,
                                       bool is_playing, int frame_count, Emotion emotion) {
    int panel_x = 398, panel_y = 275, panel_w = 232, panel_h = 195;
    int panel_r = 12;

    // 毛玻璃圆角面板
    drawFrostedRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h),
                    cv::Scalar(15, 15, 20), 0.4, 21);
    drawRoundRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h), panel_r,
                  cv::Scalar(60, 60, 80), 1);

    cv::Scalar emo_color = getEmotionColor(emotion);

    // 标题 — 用情绪色
    cv::putText(frame, "MUSIC", cv::Point(panel_x + 14, panel_y + 18),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, emo_color, 1);

    // 分隔线
    cv::line(frame, cv::Point(panel_x + 14, panel_y + 24),
             cv::Point(panel_x + panel_w - 14, panel_y + 24),
             cv::Scalar(60, 60, 80), 1);

    // 参数 — 更淡雅
    cv::Scalar param_label(120, 120, 140);
    cv::Scalar param_value(210, 210, 220);

    int py = panel_y + 42;
    int line_h = 22;

    cv::putText(frame, "Key", cv::Point(panel_x + 14, py), cv::FONT_HERSHEY_SIMPLEX, 0.32, param_label, 1);
    cv::putText(frame, params.key + " " + params.scale, cv::Point(panel_x + 80, py),
                cv::FONT_HERSHEY_SIMPLEX, 0.38, param_value, 1);

    py += line_h;
    cv::putText(frame, "Tempo", cv::Point(panel_x + 14, py), cv::FONT_HERSHEY_SIMPLEX, 0.32, param_label, 1);
    cv::putText(frame, std::to_string(params.tempo) + " BPM", cv::Point(panel_x + 80, py),
                cv::FONT_HERSHEY_SIMPLEX, 0.38, param_value, 1);

    py += line_h;
    cv::putText(frame, "Vel", cv::Point(panel_x + 14, py), cv::FONT_HERSHEY_SIMPLEX, 0.32, param_label, 1);
    cv::putText(frame, std::to_string(params.velocity), cv::Point(panel_x + 80, py),
                cv::FONT_HERSHEY_SIMPLEX, 0.38, param_value, 1);

    py += line_h;
    cv::putText(frame, "Mode", cv::Point(panel_x + 14, py), cv::FONT_HERSHEY_SIMPLEX, 0.32, param_label, 1);
    cv::putText(frame, params.accompaniment, cv::Point(panel_x + 80, py),
                cv::FONT_HERSHEY_SIMPLEX, 0.35, param_value, 1);

    // 分隔线
    py += 10;
    cv::line(frame, cv::Point(panel_x + 14, py),
             cv::Point(panel_x + panel_w - 14, py),
             cv::Scalar(60, 60, 80), 1);

    // EQ 均衡器动画 — 圆角柱状图
    int eq_y_base = panel_y + panel_h - 12;
    int bar_w = 8, bar_gap = 15;
    int eq_x_start = panel_x + 20;
    int num_bars = 8;

    for (int b = 0; b < num_bars; ++b) {
        int bar_h;
        cv::Scalar bar_color;

        if (is_playing) {
            // 平滑的动画高度
            bar_h = 8 + static_cast<int>(20 * std::abs(
                std::sin(frame_count * 0.18 + b * 0.8) *
                std::cos(frame_count * 0.07 + b * 0.5)));

            // 渐变色：从情绪色到绿色
            double t = b / static_cast<double>(num_bars);
            bar_color = cv::Scalar(
                emo_color[0] * (1 - t) + 80 * t,
                emo_color[1] * (1 - t) + 200 * t,
                emo_color[2] * (1 - t) + 120 * t
            );
        } else {
            bar_h = 4;
            bar_color = cv::Scalar(50, 50, 60);
        }

        int bx = eq_x_start + b * bar_gap;
        int by = eq_y_base - bar_h;
        cv::rectangle(frame, cv::Point(bx, by), cv::Point(bx + bar_w, eq_y_base),
                      bar_color, -1);
    }

    // 状态文字
    std::string status = is_playing ? "PLAYING" : "IDLE";
    cv::Scalar status_color = is_playing ? cv::Scalar(80, 200, 120) : cv::Scalar(90, 90, 100);
    cv::putText(frame, status, cv::Point(eq_x_start + num_bars * bar_gap + 6, eq_y_base - 4),
                cv::FONT_HERSHEY_SIMPLEX, 0.3, status_color, 1);
}

// ========== 底部提示栏 ==========

void OverlayRenderer::drawFooterBar(cv::Mat& frame) {
    int bar_h = 28;
    int bar_y = frame.rows - bar_h;

    drawFrostedRect(frame, cv::Rect(0, bar_y, frame.cols, bar_h),
                    cv::Scalar(15, 15, 20), 0.4, 21);

    // 细线上边
    cv::line(frame, cv::Point(0, bar_y), cv::Point(frame.cols, bar_y),
             cv::Scalar(60, 60, 80), 1);

    // 分段显示快捷键
    struct Shortcut { std::string key; std::string desc; };
    Shortcut shortcuts[] = {
        {"ESC", "Quit"}, {"SPACE", "Play"}, {"M", "Auto"}
    };

    cv::Scalar key_color(180, 200, 220);
    cv::Scalar desc_color(100, 100, 120);
    cv::Scalar sep_color(70, 70, 90);

    int total_w = 0;
    double font = 0.32;
    for (auto& s : shortcuts) {
        total_w += cv::getTextSize(s.key, cv::FONT_HERSHEY_SIMPLEX, font, 1, nullptr).width + 4;
        total_w += cv::getTextSize(s.desc, cv::FONT_HERSHEY_SIMPLEX, font, 1, nullptr).width + 20;
    }
    total_w -= 20;  // 最后一个不需要分隔符间距

    int x = (frame.cols - total_w) / 2;
    int text_y = bar_y + 18;

    for (int i = 0; i < 3; ++i) {
        cv::putText(frame, shortcuts[i].key, cv::Point(x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, font, key_color, 1);
        x += cv::getTextSize(shortcuts[i].key, cv::FONT_HERSHEY_SIMPLEX, font, 1, nullptr).width + 4;
        cv::putText(frame, shortcuts[i].desc, cv::Point(x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, font, desc_color, 1);
        x += cv::getTextSize(shortcuts[i].desc, cv::FONT_HERSHEY_SIMPLEX, font, 1, nullptr).width;

        if (i < 2) {
            x += 10;
            cv::putText(frame, "|", cv::Point(x, text_y),
                        cv::FONT_HERSHEY_SIMPLEX, font, sep_color, 1);
            x += 10;
        }
    }
}

// ========== 无脸提示 ==========

void OverlayRenderer::drawNoFaceHint(cv::Mat& frame) {
    std::string hint = "No face detected";
    double font_scale = 0.6;
    int thickness = 1;
    int text_w = cv::getTextSize(hint, cv::FONT_HERSHEY_SIMPLEX, font_scale, thickness, nullptr).width;
    int cx = frame.cols / 2;
    int cy = frame.rows / 2;

    int box_w = text_w + 80;
    int box_h = 60;
    int box_r = 16;

    // 毛玻璃面板
    drawFrostedRect(frame, cv::Rect(cx - box_w / 2, cy - box_h / 2, box_w, box_h),
                    cv::Scalar(10, 10, 15), 0.5, 25);
    drawRoundRect(frame, cv::Rect(cx - box_w / 2, cy - box_h / 2, box_w, box_h), box_r,
                  cv::Scalar(60, 60, 80), 1);

    // 简化人像图标（圆 + 半圆弧肩膀）
    int icon_x = cx - box_w / 2 + 28;
    int icon_y = cy - 2;
    cv::circle(frame, cv::Point(icon_x, icon_y - 10), 7, cv::Scalar(100, 100, 200), 2);
    cv::ellipse(frame, cv::Point(icon_x, icon_y + 12), cv::Size(14, 8), 0, 180, 360,
                cv::Scalar(100, 100, 200), 2);

    // 文字
    cv::putText(frame, hint, cv::Point(cx - box_w / 2 + 48, cy + 5),
                cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(150, 150, 220), thickness);
}

// ========== 总渲染入口 ==========

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
    drawMusicPanel(frame, music_params, is_playing, frame_count, current_emotion);

    // 4. 底部提示栏
    drawFooterBar(frame);

    // 5. 无脸提示（最后绘制，覆盖在其他元素之上）
    if (!face_detected) {
        drawNoFaceHint(frame);
    }
}
