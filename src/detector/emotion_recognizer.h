#ifndef EMOTION_RECOGNIZER_H
#define EMOTION_RECOGNIZER_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

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
    EmotionRecognizer();
    ~EmotionRecognizer() = default;

    // 加载 ONNX 模型（使用 onnxruntime）
    bool loadModel(const std::string& model_path);

    // 基于人脸图像识别情绪（裁剪→缩放→归一化→推理）
    Emotion recognizeFromImage(const cv::Mat& frame, const struct FaceRect& face);

    // 获取各情绪的置信度（softmax 后的概率）
    const std::vector<float>& getConfidences() const { return confidences_; }

    static const std::vector<std::string> EMOTION_LABELS;

private:
    Ort::Env env_;
    Ort::SessionOptions session_opts_;
    std::unique_ptr<Ort::Session> session_;
    bool initialized_ = false;
    std::vector<float> confidences_;

    Emotion parseEmotion(const std::string& label);
};

#endif // EMOTION_RECOGNIZER_H
