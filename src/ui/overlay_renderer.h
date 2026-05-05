#ifndef OVERLAY_RENDERER_H
#define OVERLAY_RENDERER_H

#include <opencv2/opencv.hpp>
#include <vector>
#include "detector/emotion_recognizer.h"
#include "mapper/emotion_mapper.h"

class OverlayRenderer {
public:
    OverlayRenderer() = default;

    // 每帧调用一次，绘制全部 overlay
    void render(cv::Mat& frame,
                double fps,
                Emotion current_emotion,
                const std::vector<float>& confidences,
                const MusicParams& music_params,
                bool is_playing,
                bool music_enabled,
                bool face_detected,
                int frame_count);

    // 情绪对应颜色（供外部使用，如人脸框）
    static cv::Scalar getEmotionColor(Emotion e);

private:
    // 毛玻璃效果：高斯模糊 + 半透明叠加
    void drawFrostedRect(cv::Mat& frame, const cv::Rect& roi,
                         const cv::Scalar& tint_color = cv::Scalar(25, 25, 30),
                         double alpha = 0.35, int blur_size = 21);

    // 圆角矩形填充
    void drawRoundRect(cv::Mat& frame, const cv::Rect& rect, int radius,
                       const cv::Scalar& color, int thickness = -1);

    // 发光文字（多层半透明模拟光晕）
    void drawGlowText(cv::Mat& frame, const std::string& text, cv::Point org,
                      int font_face, double font_scale, const cv::Scalar& color,
                      int thickness = 1, int glow_layers = 3);

    // 各区域绘制
    void drawHeaderBar(cv::Mat& frame, double fps, Emotion emotion,
                       bool is_playing, bool music_enabled);
    void drawConfidencePanel(cv::Mat& frame,
                             const std::vector<float>& confidences,
                             int current_idx);
    void drawMusicPanel(cv::Mat& frame, const MusicParams& params,
                        bool is_playing, int frame_count, Emotion emotion);
    void drawFooterBar(cv::Mat& frame);
    void drawNoFaceHint(cv::Mat& frame);

    static const cv::Scalar EMOTION_COLORS[7];
    static const std::string EMOTION_SHORT_NAMES[7];
};

#endif // OVERLAY_RENDERER_H
