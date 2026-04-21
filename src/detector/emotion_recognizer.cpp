#include "emotion_recognizer.h"
#include "face_detector.h"
#include <algorithm>
#include <iostream>

// FERPlus 8 类标签：Neutral, Happy, Surprise, Sad, Anger, Disgust, Fear, Contempt
static const std::vector<std::string> FERPLUS_LABELS = {
    "Neutral", "Happy", "Surprise", "Sad", "Anger", "Disgust", "Fear", "Contempt"
};

// 映射 FERPlus → 项目内部 Emotion 枚举（Contempt 归入 Neutral）
static const Emotion FERPLUS_TO_EMOTION[] = {
    Emotion::NEUTRAL,    // Neutral
    Emotion::HAPPY,      // Happy
    Emotion::SURPRISED,  // Surprise
    Emotion::SAD,        // Sad
    Emotion::ANGRY,      // Anger
    Emotion::DISGUST,    // Disgust
    Emotion::FEAR,       // Fear
    Emotion::NEUTRAL     // Contempt → Neutral
};

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

bool EmotionRecognizer::loadModel(const std::string& model_path) {
    net_ = cv::dnn::readNetFromONNX(model_path);
    if (net_.empty()) {
        std::cerr << "[EmotionRecognizer] ONNX 模型加载失败: " << model_path << std::endl;
        return false;
    }

    net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    initialized_ = true;
    std::cout << "[EmotionRecognizer] ONNX 情绪模型加载成功" << std::endl;
    return true;
}

Emotion EmotionRecognizer::parseEmotion(const std::string& label) {
    std::string lower = label;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "angry" || lower == "anger")  return Emotion::ANGRY;
    if (lower == "disgust")                    return Emotion::DISGUST;
    if (lower == "fear")                       return Emotion::FEAR;
    if (lower == "happy")                      return Emotion::HAPPY;
    if (lower == "sad")                        return Emotion::SAD;
    if (lower == "surprise" || lower == "surprised") return Emotion::SURPRISED;
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

    // 2. 预处理：灰度 → 缩放到 64x64（FERPlus 输入尺寸）
    cv::Mat gray;
    if (face_crop.channels() == 3) {
        cv::cvtColor(face_crop, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = face_crop;
    }

    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(64, 64));

    // 3. 构造 blob: [1, 1, 64, 64]，归一化到 [0, 1]
    cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0 / 255.0, cv::Size(64, 64));

    // 4. 前向推理
    net_.setInput(blob);
    cv::Mat probs = net_.forward();

    // 5. 解析 FERPlus 输出（8 类概率）→ 映射到 7 类
    // 诊断：打印原始输出 shape 和概率
    std::cout << "[DEBUG] output shape: " << probs.size() << " (dims: " << probs.dims << ")" << std::endl;
    std::cout << "[DEBUG] raw output:";
    for (int i = 0; i < probs.cols; ++i) {
        std::cout << " " << FERPLUS_LABELS[i] << "=" << probs.at<float>(0, i);
    }
    std::cout << std::endl;

    // 将 FERPlus 的 8 类概率累加到我们的 7 类中
    confidences_.assign(7, 0.0f);
    int ferplus_max = 0;
    float ferplus_max_val = 0.0f;

    for (int i = 0; i < 8; ++i) {
        float p = probs.at<float>(0, i);
        int our_idx = static_cast<int>(FERPLUS_TO_EMOTION[i]);
        confidences_[our_idx] += p;

        if (p > ferplus_max_val) {
            ferplus_max_val = p;
            ferplus_max = i;
        }
    }

    return FERPLUS_TO_EMOTION[ferplus_max];
}
