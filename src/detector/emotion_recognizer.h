#ifndef EMOTION_RECOGNIZER_H
#define EMOTION_RECOGNIZER_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

// 情绪类型枚举（与 FER+ 模型输出顺序一致）
enum class Emotion {
    NEUTRAL   = 0,
    HAPPY     = 1,
    SURPRISED = 2,
    SAD       = 3,
    ANGRY     = 4,
    DISGUST   = 5,
    FEAR      = 6
};

std::string emotionToString(Emotion e);

class EmotionRecognizer {
public:
    EmotionRecognizer() = default;
    ~EmotionRecognizer() = default;

    // 加载 ONNX 情绪识别模型
    bool loadModel(const std::string& model_path);

    // 基于人脸图像识别情绪
    Emotion recognizeFromImage(const cv::Mat& frame, const struct FaceRect& face);

    // 获取各情绪的置信度 (0-1)
    const std::vector<float>& getConfidences() const { return confidences_; }

    static const std::vector<std::string> EMOTION_LABELS;

private:
    cv::dnn::Net net_;
    bool model_loaded_ = false;
    std::vector<float> confidences_;
};

#endif // EMOTION_RECOGNIZER_H
