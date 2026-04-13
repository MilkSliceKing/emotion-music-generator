#ifndef EMOTION_RECOGNIZER_H
#define EMOTION_RECOGNIZER_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

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

    // 初始化（设置 Python 脚本路径）
    bool loadModel(const std::string& script_path);

    // 基于人脸图像识别情绪（保存裁剪图 → 调用 Python → 解析结果）
    Emotion recognizeFromImage(const cv::Mat& frame, const struct FaceRect& face);

    // 获取各情绪的置信度
    const std::vector<float>& getConfidences() const { return confidences_; }

    static const std::vector<std::string> EMOTION_LABELS;

private:
    std::string script_path_;
    bool initialized_ = false;
    std::vector<float> confidences_;
    int temp_counter_ = 0;

    Emotion parseEmotion(const std::string& label);
};

#endif // EMOTION_RECOGNIZER_H
