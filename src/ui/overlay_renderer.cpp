#include "overlay_renderer.h"
#include <cmath>
#include <algorithm>

// 情绪颜色 (BGR)
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

// ========== 构造 ==========

OverlayRenderer::OverlayRenderer() : last_click_action_(ButtonAction::NONE) {}

cv::Scalar OverlayRenderer::getEmotionColor(Emotion e) {
    int idx = static_cast<int>(e);
    if (idx >= 0 && idx < 7) return EMOTION_COLORS[idx];
    return cv::Scalar(200, 200, 200);
}

// ========== 鼠标回调 ==========

void OverlayRenderer::onMouse(int event, int x, int y, int, void* userdata) {
    auto* self = static_cast<OverlayRenderer*>(userdata);
    if (self) self->handleMouse(event, x, y);
}

void OverlayRenderer::handleMouse(int event, int x, int y) {
    // 更新悬停状态
    for (auto& btn : buttons_) {
        btn.hovered = btn.rect.contains(cv::Point(x, y));
    }

    // 点击检测
    if (event == cv::EVENT_LBUTTONDOWN) {
        for (const auto& btn : buttons_) {
            if (btn.rect.contains(cv::Point(x, y))) {
                last_click_action_ = btn.action;
                return;
            }
        }
    }
}

// ========== 基础绘制工具 ==========

void OverlayRenderer::drawFrostedRect(cv::Mat& frame, const cv::Rect& roi,
                                        const cv::Scalar& tint_color,
                                        double alpha, int blur_size) {
    cv::Rect clamped = roi & cv::Rect(0, 0, frame.cols, frame.rows);
    if (clamped.area() <= 0) return;

    int margin = blur_size / 2 + 1;
    cv::Rect blur_roi = clamped;
    blur_roi.x = std::max(0, blur_roi.x - margin);
    blur_roi.y = std::max(0, blur_roi.y - margin);
    blur_roi.width = std::min(frame.cols - blur_roi.x, clamped.width + 2 * margin);
    blur_roi.height = std::min(frame.rows - blur_roi.y, clamped.height + 2 * margin);

    cv::Mat blurred;
    cv::GaussianBlur(frame(blur_roi), blurred, cv::Size(blur_size, blur_size), 0);

    cv::Rect inner(clamped.x - blur_roi.x, clamped.y - blur_roi.y, clamped.width, clamped.height);
    cv::Mat blurred_region = blurred(inner);

    cv::Mat tinted = blurred_region.clone();
    cv::rectangle(tinted, cv::Point(0, 0), cv::Point(clamped.width, clamped.height),
                  tint_color, -1, cv::LINE_AA);
    cv::addWeighted(tinted, alpha, blurred_region, 1.0 - alpha, 0, frame(clamped));
}

void OverlayRenderer::drawRoundRect(cv::Mat& frame, const cv::Rect& rect, int radius,
                                      const cv::Scalar& color, int thickness) {
    int r = std::min({radius, rect.width / 2, rect.height / 2});
    if (r < 1) r = 1;
    cv::Point tl(rect.x + r, rect.y + r);
    cv::Point br(rect.x + rect.width - r, rect.y + rect.height - r);

    if (thickness == -1) {
        cv::ellipse(frame, tl, cv::Size(r, r), 180, 0, 90, color, -1, cv::LINE_AA);
        cv::ellipse(frame, cv::Point(br.x, tl.y), cv::Size(r, r), 270, 0, 90, color, -1, cv::LINE_AA);
        cv::ellipse(frame, br, cv::Size(r, r), 0, 0, 90, color, -1, cv::LINE_AA);
        cv::ellipse(frame, cv::Point(tl.x, br.y), cv::Size(r, r), 90, 0, 90, color, -1, cv::LINE_AA);
        cv::rectangle(frame, cv::Point(tl.x, rect.y), cv::Point(br.x, rect.y + rect.height), color, -1, cv::LINE_AA);
        cv::rectangle(frame, cv::Point(rect.x, tl.y), cv::Point(rect.x + rect.width, br.y), color, -1, cv::LINE_AA);
    } else {
        cv::ellipse(frame, tl, cv::Size(r, r), 180, 0, 90, color, thickness, cv::LINE_AA);
        cv::ellipse(frame, cv::Point(br.x, tl.y), cv::Size(r, r), 270, 0, 90, color, thickness, cv::LINE_AA);
        cv::ellipse(frame, br, cv::Size(r, r), 0, 0, 90, color, thickness, cv::LINE_AA);
        cv::ellipse(frame, cv::Point(tl.x, br.y), cv::Size(r, r), 90, 0, 90, color, thickness, cv::LINE_AA);
        cv::line(frame, cv::Point(tl.x, rect.y), cv::Point(br.x, rect.y), color, thickness, cv::LINE_AA);
        cv::line(frame, cv::Point(tl.x, rect.y + rect.height), cv::Point(br.x, rect.y + rect.height), color, thickness, cv::LINE_AA);
        cv::line(frame, cv::Point(rect.x, tl.y), cv::Point(rect.x, br.y), color, thickness, cv::LINE_AA);
        cv::line(frame, cv::Point(rect.x + rect.width, tl.y), cv::Point(rect.x + rect.width, br.y), color, thickness, cv::LINE_AA);
    }
}

void OverlayRenderer::drawGlowText(cv::Mat& frame, const std::string& text, cv::Point org,
                                     int font_face, double font_scale, const cv::Scalar& color,
                                     int thickness, int glow_layers) {
    for (int i = glow_layers; i >= 1; --i) {
        double alpha = 0.12 / i;
        cv::Scalar glow_color(
            static_cast<double>(color[0]) * alpha + 100 * (1 - alpha),
            static_cast<double>(color[1]) * alpha + 100 * (1 - alpha),
            static_cast<double>(color[2]) * alpha + 100 * (1 - alpha)
        );
        cv::putText(frame, text, org, font_face, font_scale, glow_color, thickness + i * 2, cv::LINE_AA);
    }
    cv::putText(frame, text, org, font_face, font_scale, color, thickness, cv::LINE_AA);
}

// ========== 顶部状态栏 ==========

void OverlayRenderer::drawHeaderBar(cv::Mat& frame, double fps, Emotion emotion,
                                      bool is_playing, bool music_enabled) {
    int bar_h = 60;
    drawFrostedRect(frame, cv::Rect(0, 0, frame.cols, bar_h), cv::Scalar(15, 15, 20), 0.45, 25);
    cv::line(frame, cv::Point(0, bar_h - 1), cv::Point(frame.cols, bar_h - 1),
             cv::Scalar(80, 80, 100), 1, cv::LINE_AA);

    // FPS
    std::string fps_text = std::to_string(static_cast<int>(fps)) + " fps";
    cv::putText(frame, fps_text, cv::Point(16, 26), cv::FONT_HERSHEY_SIMPLEX, 0.35,
                cv::Scalar(120, 120, 130), 1, cv::LINE_AA);

    // 播放状态胶囊
    int pill_x = 90, pill_y = 10, pill_w = 70, pill_h = 20, pill_r = 10;
    cv::Scalar pill_bg = is_playing ? cv::Scalar(20, 130, 60) : cv::Scalar(50, 50, 60);
    drawRoundRect(frame, cv::Rect(pill_x, pill_y, pill_w, pill_h), pill_r, pill_bg, -1);
    if (is_playing) {
        cv::rectangle(frame, cv::Point(pill_x + 10, pill_y + 5), cv::Point(pill_x + 14, pill_y + 15),
                      cv::Scalar(255, 255, 255), -1, cv::LINE_AA);
        cv::rectangle(frame, cv::Point(pill_x + 18, pill_y + 5), cv::Point(pill_x + 22, pill_y + 15),
                      cv::Scalar(255, 255, 255), -1, cv::LINE_AA);
    } else {
        cv::Point pts[3] = {{pill_x + 11, pill_y + 5}, {pill_x + 11, pill_y + 15}, {pill_x + 23, pill_y + 10}};
        cv::fillConvexPoly(frame, pts, 3, cv::Scalar(255, 255, 255), cv::LINE_AA);
    }
    cv::putText(frame, is_playing ? "PLAY" : "IDLE", cv::Point(pill_x + 28, pill_y + 15),
                cv::FONT_HERSHEY_SIMPLEX, 0.32, cv::Scalar(240, 240, 240), 1, cv::LINE_AA);

    // 自动播放
    int auto_x = frame.cols - 100, auto_y = 10;
    cv::Scalar auto_bg = music_enabled ? cv::Scalar(20, 130, 60) : cv::Scalar(50, 50, 60);
    drawRoundRect(frame, cv::Rect(auto_x, auto_y, 85, 20), 10, auto_bg, -1);
    int dot_x = music_enabled ? auto_x + 67 : auto_x + 18;
    cv::circle(frame, cv::Point(dot_x, auto_y + 10), 7, cv::Scalar(255, 255, 255), -1, cv::LINE_AA);
    cv::putText(frame, music_enabled ? "ON" : "OFF",
                cv::Point(music_enabled ? auto_x + 12 : auto_x + 40, auto_y + 15),
                cv::FONT_HERSHEY_SIMPLEX, 0.32, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

    // 情绪标签
    std::string emo_text = emotionToString(emotion);
    cv::Scalar emo_color = getEmotionColor(emotion);
    drawGlowText(frame, emo_text, cv::Point(16, 52), cv::FONT_HERSHEY_SIMPLEX, 0.75, emo_color, 2, 3);

    cv::circle(frame, cv::Point(frame.cols - 20, 30), 8, emo_color, -1, cv::LINE_AA);
    cv::circle(frame, cv::Point(frame.cols - 20, 30), 10, emo_color, 2, cv::LINE_AA);
}

// ========== 置信度面板 ==========

void OverlayRenderer::drawConfidencePanel(cv::Mat& frame,
                                            const std::vector<float>& confidences,
                                            int current_idx) {
    int panel_x = 10, panel_y = 275, panel_w = 240, panel_h = 195;
    drawFrostedRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h), cv::Scalar(15, 15, 20), 0.4, 21);
    drawRoundRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h), 12, cv::Scalar(60, 60, 80), 1);

    if (confidences.size() < 7) return;

    cv::putText(frame, "CONFIDENCE", cv::Point(panel_x + 14, panel_y + 18),
                cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(140, 140, 160), 1, cv::LINE_AA);

    int bar_y_start = panel_y + 28;
    int bar_h = 14, bar_spacing = 22;
    int label_x = panel_x + 10, bar_x = panel_x + 58, bar_max_w = 140, pct_x = panel_x + 204;

    for (int i = 0; i < 7; ++i) {
        int y = bar_y_start + i * bar_spacing;
        int text_y = y + bar_h - 2;
        bool is_top = (i == current_idx);

        cv::Scalar label_color = is_top ? cv::Scalar(230, 230, 240) : cv::Scalar(130, 130, 140);
        cv::putText(frame, EMOTION_SHORT_NAMES[i], cv::Point(label_x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.33, label_color, 1, cv::LINE_AA);

        cv::rectangle(frame, cv::Point(bar_x, y + 2), cv::Point(bar_x + bar_max_w, y + bar_h - 2),
                      cv::Scalar(40, 40, 50), -1, cv::LINE_AA);

        int bar_w = static_cast<int>(confidences[i] * bar_max_w);
        if (bar_w > 3) {
            cv::Scalar bar_color = EMOTION_COLORS[i];
            if (is_top) {
                bar_color = cv::Scalar(std::min(255.0, bar_color[0] * 1.3),
                                       std::min(255.0, bar_color[1] * 1.3),
                                       std::min(255.0, bar_color[2] * 1.3));
            }
            cv::rectangle(frame, cv::Point(bar_x, y + 2), cv::Point(bar_x + bar_w, y + bar_h - 2),
                          bar_color, -1, cv::LINE_AA);
        }

        int pct = static_cast<int>(confidences[i] * 100);
        cv::Scalar pct_color = is_top ? cv::Scalar(255, 255, 255) : cv::Scalar(150, 150, 160);
        cv::putText(frame, std::to_string(pct) + "%", cv::Point(pct_x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.35, pct_color, is_top ? 2 : 1, cv::LINE_AA);
    }
}

// ========== 音乐面板 ==========

void OverlayRenderer::drawMusicPanel(cv::Mat& frame, const MusicParams& params,
                                       bool is_playing, int frame_count, Emotion emotion) {
    int panel_x = 398, panel_y = 275, panel_w = 232, panel_h = 195;
    drawFrostedRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h), cv::Scalar(15, 15, 20), 0.4, 21);
    drawRoundRect(frame, cv::Rect(panel_x, panel_y, panel_w, panel_h), 12, cv::Scalar(60, 60, 80), 1);

    cv::Scalar emo_color = getEmotionColor(emotion);
    cv::putText(frame, "MUSIC", cv::Point(panel_x + 14, panel_y + 18),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, emo_color, 1, cv::LINE_AA);
    cv::line(frame, cv::Point(panel_x + 14, panel_y + 24), cv::Point(panel_x + panel_w - 14, panel_y + 24),
             cv::Scalar(60, 60, 80), 1, cv::LINE_AA);

    cv::Scalar param_label(120, 120, 140), param_value(210, 210, 220);
    int py = panel_y + 42, line_h = 22;

    cv::putText(frame, "Key", cv::Point(panel_x + 14, py), cv::FONT_HERSHEY_SIMPLEX, 0.32, param_label, 1, cv::LINE_AA);
    cv::putText(frame, params.key + " " + params.scale, cv::Point(panel_x + 80, py),
                cv::FONT_HERSHEY_SIMPLEX, 0.38, param_value, 1, cv::LINE_AA);

    py += line_h;
    cv::putText(frame, "Tempo", cv::Point(panel_x + 14, py), cv::FONT_HERSHEY_SIMPLEX, 0.32, param_label, 1, cv::LINE_AA);
    cv::putText(frame, std::to_string(params.tempo) + " BPM", cv::Point(panel_x + 80, py),
                cv::FONT_HERSHEY_SIMPLEX, 0.38, param_value, 1, cv::LINE_AA);

    py += line_h;
    cv::putText(frame, "Vel", cv::Point(panel_x + 14, py), cv::FONT_HERSHEY_SIMPLEX, 0.32, param_label, 1, cv::LINE_AA);
    cv::putText(frame, std::to_string(params.velocity), cv::Point(panel_x + 80, py),
                cv::FONT_HERSHEY_SIMPLEX, 0.38, param_value, 1, cv::LINE_AA);

    py += line_h;
    cv::putText(frame, "Mode", cv::Point(panel_x + 14, py), cv::FONT_HERSHEY_SIMPLEX, 0.32, param_label, 1, cv::LINE_AA);
    cv::putText(frame, params.accompaniment, cv::Point(panel_x + 80, py),
                cv::FONT_HERSHEY_SIMPLEX, 0.35, param_value, 1, cv::LINE_AA);

    py += 10;
    cv::line(frame, cv::Point(panel_x + 14, py), cv::Point(panel_x + panel_w - 14, py),
             cv::Scalar(60, 60, 80), 1, cv::LINE_AA);

    // EQ 均衡器
    int eq_y_base = panel_y + panel_h - 12;
    int bw = 8, bgap = 15, eq_x = panel_x + 20, nbars = 8;
    for (int b = 0; b < nbars; ++b) {
        int bh;
        cv::Scalar bcolor;
        if (is_playing) {
            bh = 8 + static_cast<int>(20 * std::abs(
                std::sin(frame_count * 0.18 + b * 0.8) * std::cos(frame_count * 0.07 + b * 0.5)));
            double t = b / static_cast<double>(nbars);
            bcolor = cv::Scalar(emo_color[0] * (1 - t) + 80 * t,
                                emo_color[1] * (1 - t) + 200 * t,
                                emo_color[2] * (1 - t) + 120 * t);
        } else {
            bh = 4;
            bcolor = cv::Scalar(50, 50, 60);
        }
        cv::rectangle(frame, cv::Point(eq_x + b * bgap, eq_y_base - bh),
                      cv::Point(eq_x + b * bgap + bw, eq_y_base), bcolor, -1, cv::LINE_AA);
    }

    cv::Scalar sc = is_playing ? cv::Scalar(80, 200, 120) : cv::Scalar(90, 90, 100);
    cv::putText(frame, is_playing ? "PLAYING" : "IDLE",
                cv::Point(eq_x + nbars * bgap + 6, eq_y_base - 4),
                cv::FONT_HERSHEY_SIMPLEX, 0.3, sc, 1, cv::LINE_AA);
}

// ========== 情绪历史曲线图 ==========

void OverlayRenderer::drawEmotionChart(cv::Mat& frame, const std::deque<Emotion>& history,
                                          Emotion current_emotion) {
    int chart_x = 260, chart_y = 275, chart_w = 128, chart_h = 100;

    drawFrostedRect(frame, cv::Rect(chart_x, chart_y, chart_w, chart_h), cv::Scalar(15, 15, 20), 0.4, 21);
    drawRoundRect(frame, cv::Rect(chart_x, chart_y, chart_w, chart_h), 12, cv::Scalar(60, 60, 80), 1);

    // 标题
    cv::putText(frame, "HISTORY", cv::Point(chart_x + 10, chart_y + 16),
                cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(140, 140, 160), 1, cv::LINE_AA);

    if (history.empty()) return;

    // 绘制区域
    int draw_x = chart_x + 8, draw_y = chart_y + 22;
    int draw_w = chart_w - 16, draw_h = chart_h - 34;

    int n = static_cast<int>(history.size());
    int max_pts = std::min(n, draw_w);

    // 为每种情绪绘制点线
    for (int e = 0; e < 7; ++e) {
        cv::Scalar color = EMOTION_COLORS[e];
        // 降低非当前情绪的透明度
        if (e != static_cast<int>(current_emotion)) {
            color = cv::Scalar(color[0] * 0.4, color[1] * 0.4, color[2] * 0.4);
        }

        std::vector<cv::Point> pts;
        for (int i = 0; i < max_pts; ++i) {
            int hist_idx = n - max_pts + i;
            if (hist_idx < 0) continue;
            int x = draw_x + i;
            // Y: 当前情绪=e 时在高处，否则在低处
            int y = (static_cast<int>(history[hist_idx]) == e)
                    ? draw_y + 2
                    : draw_y + draw_h - 2;
            pts.push_back(cv::Point(x, y));
        }

        if (pts.size() > 1) {
            // 绘制连续线段（只连接相邻同情绪的高点）
            for (size_t j = 1; j < pts.size(); ++j) {
                int idx1 = n - max_pts + (j - 1);
                int idx2 = n - max_pts + j;
                if (idx1 >= 0 && idx2 >= 0 &&
                    static_cast<int>(history[idx1]) == e &&
                    static_cast<int>(history[idx2]) == e) {
                    cv::line(frame, pts[j - 1], pts[j], color, 1, cv::LINE_AA);
                }
                // 画点
                if (static_cast<int>(history[std::max(0, n - max_pts + static_cast<int>(j))]) == e) {
                    cv::circle(frame, pts[j], 1, color, -1, cv::LINE_AA);
                }
            }
        }
    }

    // 当前情绪名
    cv::Scalar ec = getEmotionColor(current_emotion);
    cv::putText(frame, emotionToString(current_emotion),
                cv::Point(chart_x + 10, chart_y + chart_h - 6),
                cv::FONT_HERSHEY_SIMPLEX, 0.32, ec, 1, cv::LINE_AA);
}

// ========== 屏幕按钮 ==========

void OverlayRenderer::drawButtons(cv::Mat& frame, bool is_playing, bool music_enabled, int playback_mode, bool web_on, int web_port) {
    int btn_w = 58, btn_h = 26, btn_r = 13, gap = 6;
    int start_x = 10;
    int btn_y = frame.rows - 60;

    // 更新按钮列表（4个按钮）
    buttons_ = {
        {{start_x, btn_y, btn_w, btn_h}, "QUIT", ButtonAction::QUIT, false, false},
        {{start_x + btn_w + gap, btn_y, btn_w, btn_h}, "PLAY", ButtonAction::PLAY, is_playing, false},
        {{start_x + 2 * (btn_w + gap), btn_y, btn_w + 6, btn_h}, "AUTO", ButtonAction::AUTO_TOGGLE, music_enabled, false},
        {{start_x + 3 * (btn_w + gap) + 6, btn_y, btn_w + 10, btn_h},
         playback_mode == 0 ? "SYNTH" : "LOCAL", ButtonAction::MODE_TOGGLE, playback_mode == 1, false}
    };

    for (auto& btn : buttons_) {
        cv::Scalar bg_color, text_color;
        if (btn.action == ButtonAction::QUIT) {
            bg_color = btn.hovered ? cv::Scalar(40, 40, 180) : cv::Scalar(30, 30, 120);
            text_color = cv::Scalar(200, 200, 255);
        } else if (btn.action == ButtonAction::PLAY) {
            bg_color = btn.active
                ? (btn.hovered ? cv::Scalar(20, 180, 80) : cv::Scalar(20, 140, 60))
                : (btn.hovered ? cv::Scalar(60, 80, 60) : cv::Scalar(40, 55, 40));
            text_color = btn.active ? cv::Scalar(200, 255, 200) : cv::Scalar(140, 180, 140);
        } else if (btn.action == ButtonAction::MODE_TOGGLE) {
            bg_color = btn.active
                ? (btn.hovered ? cv::Scalar(160, 50, 160) : cv::Scalar(120, 30, 120))
                : (btn.hovered ? cv::Scalar(60, 60, 100) : cv::Scalar(45, 45, 75));
            text_color = btn.active ? cv::Scalar(255, 200, 255) : cv::Scalar(170, 170, 210);
        } else {
            bg_color = btn.active
                ? (btn.hovered ? cv::Scalar(20, 180, 80) : cv::Scalar(20, 140, 60))
                : (btn.hovered ? cv::Scalar(70, 70, 80) : cv::Scalar(50, 50, 60));
            text_color = btn.active ? cv::Scalar(200, 255, 200) : cv::Scalar(160, 160, 170);
        }

        drawRoundRect(frame, btn.rect, btn_r, bg_color, -1);
        // 悬停时亮边框
        if (btn.hovered) {
            drawRoundRect(frame, btn.rect, btn_r, cv::Scalar(180, 180, 200), 2);
        }

        // 标签
        int tw = cv::getTextSize(btn.label, cv::FONT_HERSHEY_SIMPLEX, 0.35, 1, nullptr).width;
        cv::Point text_org(btn.rect.x + (btn.rect.width - tw) / 2,
                           btn.rect.y + btn.rect.height / 2 + 5);
        cv::putText(frame, btn.label, text_org, cv::FONT_HERSHEY_SIMPLEX, 0.35,
                    text_color, 1, cv::LINE_AA);
    }
}

// ========== 底部提示栏 ==========

void OverlayRenderer::drawFooterBar(cv::Mat& frame, bool web_on, int web_port) {
    int bar_h = 28, bar_y = frame.rows - bar_h;
    drawFrostedRect(frame, cv::Rect(0, bar_y, frame.cols, bar_h), cv::Scalar(15, 15, 20), 0.4, 21);
    cv::line(frame, cv::Point(0, bar_y), cv::Point(frame.cols, bar_y), cv::Scalar(60, 60, 80), 1, cv::LINE_AA);

    struct Shortcut { std::string key; std::string desc; };
    Shortcut shortcuts[] = {{"ESC", "Quit"}, {"SPACE", "Play"}, {"M", "Auto"}, {"L", "Mode"}};
    cv::Scalar key_color(180, 200, 220), desc_color(100, 100, 120), sep_color(70, 70, 90);

    int total_w = 0;
    double font = 0.32;
    for (auto& s : shortcuts) {
        total_w += cv::getTextSize(s.key, cv::FONT_HERSHEY_SIMPLEX, font, 1, nullptr).width + 4;
        total_w += cv::getTextSize(s.desc, cv::FONT_HERSHEY_SIMPLEX, font, 1, nullptr).width + 20;
    }
    total_w -= 20;

    int x = (frame.cols - total_w) / 2;
    int text_y = bar_y + 18;
    for (int i = 0; i < 4; ++i) {
        cv::putText(frame, shortcuts[i].key, cv::Point(x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, font, key_color, 1, cv::LINE_AA);
        x += cv::getTextSize(shortcuts[i].key, cv::FONT_HERSHEY_SIMPLEX, font, 1, nullptr).width + 4;
        cv::putText(frame, shortcuts[i].desc, cv::Point(x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, font, desc_color, 1, cv::LINE_AA);
        x += cv::getTextSize(shortcuts[i].desc, cv::FONT_HERSHEY_SIMPLEX, font, 1, nullptr).width;
        if (i < 3) {
            x += 10;
            cv::putText(frame, "|", cv::Point(x, text_y), cv::FONT_HERSHEY_SIMPLEX, font, sep_color, 1, cv::LINE_AA);
            x += 10;
        }
    }

    // Web 地址显示（右下角）
    if (web_on) {
        std::string web_text = "Web: :" + std::to_string(web_port);
        int tw = cv::getTextSize(web_text, cv::FONT_HERSHEY_SIMPLEX, font, 1, nullptr).width;
        cv::putText(frame, web_text, cv::Point(frame.cols - tw - 10, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, font, cv::Scalar(80, 180, 80), 1, cv::LINE_AA);
    }
}

// ========== 总渲染入口 ==========

ButtonAction OverlayRenderer::render(cv::Mat& frame,
                              double fps,
                              Emotion current_emotion,
                              const std::vector<float>& confidences,
                              const MusicParams& music_params,
                              bool is_playing,
                              bool music_enabled,
                              bool face_detected,
                              int frame_count,
                              const std::deque<Emotion>& emotion_history) {
    // 重置点击动作
    ButtonAction action = last_click_action_;
    last_click_action_ = ButtonAction::NONE;

    // 1. 顶部状态栏
    drawHeaderBar(frame, fps, current_emotion, is_playing, music_enabled);

    // 2. 置信度面板
    if (face_detected && confidences.size() >= 7) {
        drawConfidencePanel(frame, confidences, static_cast<int>(current_emotion));
    }

    // 3. 情绪曲线图
    drawEmotionChart(frame, emotion_history, current_emotion);

    // 4. 音乐面板
    drawMusicPanel(frame, music_params, is_playing, frame_count, current_emotion);

    // 5. 屏幕按钮
    drawButtons(frame, is_playing, music_enabled, 0, false, 8080);

    // 6. 底部提示栏
    drawFooterBar(frame, false, 8080);

    return action;
}
