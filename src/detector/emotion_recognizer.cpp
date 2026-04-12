#include "emotion_recognizer.h"
#include "face_detector.h"
#include <algorithm>
#include <iostream>

const std::vector<std::string> EmotionRecognizer::EMOTION_LABELS = {
    "Neutral", "Happy", "Surprise", "Sad", "Angry", "Disgust", "Fear"
};

std::string emotionToString(Emotion e) {
    int idx = static_cast<int>(e);
    if (idx >= 0 && idx < static_cast<int>(EmotionRecognizer::EMOTION_LABELS.size())) {
        return EmotionRecognizer::EMOTION_LABELS[idx];
    }
    return "Unknown";
}

bool EmotionRecognizer::loadModel(const std::string& model_path) {
    try {
        net_ = cv::dnn::readNetFromONNX(model_path);
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        model_loaded_ = true;
        std::cout << "[EmotionRecognizer] 模型加载成功: " << model_path << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "[EmotionRecognizer] 模型加载失败: " << e.what() << std::endl;
        return false;
    }
}

Emotion EmotionRecognizer::recognize(const std::vector<cv::Point>& /*landmarks*/) {
    // 旧接口保留，DNN 模式下请使用 recognizeFromImage
    return Emotion::NEUTRAL;
}

Emotion EmotionRecognizer::recognizeFromImage(const cv::Mat& frame, const FaceRect& face) {
    if (!model_loaded_) {
        return Emotion::NEUTRAL;
    }

    // 1. 裁剪人脸区域
    int x = std::max(0, face.x);
    int y = std::max(0, face.y);
    int w = std::min(frame.cols - x, face.width);
    int h = std::min(frame.rows - y, face.height);

    cv::Rect roi(x, y, w, h);
    roi &= cv::Rect(0, 0, frame.cols, frame.rows); // 防止越界

    cv::Mat face_crop = frame(roi);

    // 2. 转灰度 + 缩放到 64x64（FER+ 模型输入尺寸）
    cv::Mat gray, resized;
    cv::cvtColor(face_crop, gray, cv::COLOR_BGR2GRAY);
    cv::resize(gray, resized, cv::Size(64, 64));

    // 3. 构造输入 blob (1, 1, 64, 64)，归一化到 [0, 1]
    cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0 / 255.0, cv::Size(64, 64),
                                           cv::Scalar(0), true, false);

    // 4. 前向推理
    net_.setInput(blob);
    cv::Mat output = net_.forward();

    // 5. 解析输出 — softmax 概率
    confidences_.resize(output.cols);
    float max_val = 0;
    int max_idx = 0;
    for (int i = 0; i < output.cols; ++i) {
        confidences_[i] = output.at<float>(0, i);
        if (confidences_[i] > max_val) {
            max_val = confidences_[i];
            max_idx = i;
        }
    }

    return static_cast<Emotion>(max_idx);
}
