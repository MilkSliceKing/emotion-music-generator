#ifndef EMOTION_RECOGNIZER_H
#define EMOTION_RECOGNIZER_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

// 情绪类型枚举
enum class Emotion {
    ANGRY     = 0,
    DISGUST   = 1,
    FEAR      = 2,
    HAPPY     = 3,
    SAD       = 4,
    SURPRISED = 5,
    NEUTRAL   = 6
};

std::string emotionToString(Emotion e);

class EmotionRecognizer {
public:
    EmotionRecognizer() = default;
    ~EmotionRecognizer() = default;

    // 加载 OpenCV DNN 情绪模型（ONNX）
    bool loadModel(const std::string& model_path);

    // 基于人脸图像识别情绪（裁剪→灰度→缩放→DNN推理）
    Emotion recognizeFromImage(const cv::Mat& frame, const struct FaceRect& face);

    // 获取各情绪的置信度
    const std::vector<float>& getConfidences() const { return confidences_; }

    static const std::vector<std::string> EMOTION_LABELS;

private:
    cv::dnn::Net net_;
    bool initialized_ = false;
    std::vector<float> confidences_;

    Emotion parseEmotion(const std::string& label);
};

#endif // EMOTION_RECOGNIZER_H
