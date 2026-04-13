#include "emotion_recognizer.h"
#include "face_detector.h"
#include <algorithm>
#include <iostream>

// 标签顺序与 Caffe FER2013 模型输出一致
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

bool EmotionRecognizer::loadModel(const std::string& model_dir) {
    std::string prototxt = model_dir + "/emotion_deploy.prototxt";
    std::string caffemodel = model_dir + "/emotion_model.caffemodel";

    try {
        net_ = cv::dnn::readNetFromCaffe(prototxt, caffemodel);
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        model_loaded_ = true;
        std::cout << "[EmotionRecognizer] Caffe 情绪模型加载成功" << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "[EmotionRecognizer] 模型加载失败: " << e.what() << std::endl;
        return false;
    }
}

Emotion EmotionRecognizer::recognizeFromImage(const cv::Mat& frame, const FaceRect& face) {
    if (!model_loaded_) {
        return Emotion::NEUTRAL;
    }

    // 1. 裁剪人脸区域，加 20% 边距让五官更完整
    float margin = 0.2f;
    int margin_x = static_cast<int>(face.width * margin);
    int margin_y = static_cast<int>(face.height * margin);

    int x = std::max(0, face.x - margin_x);
    int y = std::max(0, face.y - margin_y);
    int w = std::min(frame.cols - x, face.width + 2 * margin_x);
    int h = std::min(frame.rows - y, face.height + 2 * margin_y);

    cv::Rect roi(x, y, w, h);
    roi &= cv::Rect(0, 0, frame.cols, frame.rows);

    cv::Mat face_crop = frame(roi);

    // 2. 转灰度 + 缩放到 48x48（FER2013 标准输入）
    cv::Mat gray, resized;
    cv::cvtColor(face_crop, gray, cv::COLOR_BGR2GRAY);
    cv::resize(gray, resized, cv::Size(48, 48));

    // 3. 构造 blob: (1, 1, 48, 48)，不做归一化（Caffe FER 模型训练时的处理方式）
    cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0, cv::Size(48, 48),
                                           cv::Scalar(0), false, false);

    // 4. 前向推理
    net_.setInput(blob);
    cv::Mat output = net_.forward();

    // 5. softmax 得到概率分布
    confidences_.resize(output.cols);
    float sum = 0;
    float max_val = -1;
    int max_idx = 0;

    for (int i = 0; i < output.cols; ++i) {
        confidences_[i] = std::exp(output.at<float>(0, i));
        sum += confidences_[i];
    }
    for (int i = 0; i < output.cols; ++i) {
        confidences_[i] /= sum;
        if (confidences_[i] > max_val) {
            max_val = confidences_[i];
            max_idx = i;
        }
    }

    return static_cast<Emotion>(max_idx);
}
