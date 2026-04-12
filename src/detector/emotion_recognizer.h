#ifndef EMOTION_RECOGNIZER_H
#define EMOTION_RECOGNIZER_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

// 情绪类型枚举
enum class Emotion {
    HAPPY,
    SAD,
    ANGRY,
    SURPRISED,
    NEUTRAL,
    FEAR,
    DISGUST
};

std::string emotionToString(Emotion e);

class EmotionRecognizer {
public:
    EmotionRecognizer() = default;
    ~EmotionRecognizer() = default;

    // 基于 68 关键点识别情绪
    Emotion recognize(const std::vector<cv::Point>& landmarks);

private:
    // 面部特征计算
    double getMouthAspectRatio(const std::vector<cv::Point>& landmarks);
    double getEyeAspectRatio(const std::vector<cv::Point>& landmarks, bool left);
    double getEyebrowHeight(const std::vector<cv::Point>& landmarks, bool left);
    double getMouthCornerAngle(const std::vector<cv::Point>& landmarks);
    double distance(const cv::Point& p1, const cv::Point& p2);
};

#endif // EMOTION_RECOGNIZER_H
