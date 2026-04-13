#include "emotion_recognizer.h"
#include "face_detector.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sstream>

const std::vector<std::string> EmotionRecognizer::EMOTION_LABELS = {
    "Angry", "Disgust", "Fear", "Happy", "Sad", "Surprise", "Neutral"
};

std::string emotionToString(Emotion e) {
    int idx = static_cast<int>(e);
    if (idx >= 0 && idx < static_cast<int>(EmotionRecognizer::EMOTION_LABELS.size())) {
        return EmotionRecognizer::EMOTION_LABELS[idx];
    }
    return "Unknown";
}

bool EmotionRecognizer::loadModel(const std::string& script_path) {
    script_path_ = script_path;
    initialized_ = true;
    std::cout << "[EmotionRecognizer] 使用 DeepFace 桥接脚本: " << script_path << std::endl;
    return true;
}

Emotion EmotionRecognizer::parseEmotion(const std::string& label) {
    std::string lower = label;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "angry")     return Emotion::ANGRY;
    if (lower == "disgust")   return Emotion::DISGUST;
    if (lower == "fear")      return Emotion::FEAR;
    if (lower == "happy")     return Emotion::HAPPY;
    if (lower == "sad")       return Emotion::SAD;
    if (lower == "surprise")  return Emotion::SURPRISED;
    return Emotion::NEUTRAL;
}

Emotion EmotionRecognizer::recognizeFromImage(const cv::Mat& frame, const FaceRect& face) {
    if (!initialized_) return Emotion::NEUTRAL;

    // 1. 裁剪人脸，加 20% 边距
    float margin = 0.2f;
    int mx = static_cast<int>(face.width * margin);
    int my = static_cast<int>(face.height * margin);

    int x = std::max(0, face.x - mx);
    int y = std::max(0, face.y - my);
    int w = std::min(frame.cols - x, face.width + 2 * mx);
    int h = std::min(frame.rows - y, face.height + 2 * my);

    cv::Rect roi(x, y, w, h);
    roi &= cv::Rect(0, 0, frame.cols, frame.rows);

    cv::Mat face_crop = frame(roi);

    // 2. 保存为临时文件
    std::string temp_path = "/tmp/emo_face_" + std::to_string(temp_counter_++) + ".jpg";
    cv::imwrite(temp_path, face_crop);

    // 3. 调用 Python 脚本
    std::string cmd = "python3 " + script_path_ + " " + temp_path + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "[EmotionRecognizer] 无法执行 Python 脚本" << std::endl;
        std::remove(temp_path.c_str());
        return Emotion::NEUTRAL;
    }

    char buffer[512];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);

    // 删除临时文件
    std::remove(temp_path.c_str());

    // 4. 解析输出
    // 格式: dominant_emotion,angry,disgust,fear,happy,sad,surprise,neutral
    if (result.empty() || result.substr(0, 5) == "ERROR") {
        return Emotion::NEUTRAL;
    }

    // 去掉换行
    if (!result.empty() && result.back() == '\n') result.pop_back();

    std::stringstream ss(result);
    std::string token;
    std::vector<std::string> parts;
    while (std::getline(ss, token, ',')) {
        parts.push_back(token);
    }

    if (parts.size() < 8) return Emotion::NEUTRAL;

    // parts[0] = dominant emotion label
    // parts[1-7] = angry,disgust,fear,happy,sad,surprise,neutral 置信度
    confidences_.resize(7);
    for (int i = 0; i < 7; ++i) {
        confidences_[i] = std::stof(parts[i + 1]) / 100.0f;
    }

    return parseEmotion(parts[0]);
}
