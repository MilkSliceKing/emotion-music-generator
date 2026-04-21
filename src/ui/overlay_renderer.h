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
    // 半透明矩形
    void drawTransRect(cv::Mat& frame, const cv::Rect& roi,
                       const cv::Scalar& color, double alpha);

    // 各区域绘制
    void drawHeaderBar(cv::Mat& frame, double fps, Emotion emotion,
                       bool is_playing, bool music_enabled);
    void drawConfidencePanel(cv::Mat& frame,
                             const std::vector<float>& confidences,
                             int current_idx);
    void drawMusicPanel(cv::Mat& frame, const MusicParams& params,
                        bool is_playing, int frame_count);
    void drawFooterBar(cv::Mat& frame);
    void drawNoFaceHint(cv::Mat& frame);

    static const cv::Scalar EMOTION_COLORS[7];
    static const std::string EMOTION_SHORT_NAMES[7];
};

#endif // OVERLAY_RENDERER_H
