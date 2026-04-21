#include "emotion_recognizer.h"
#include "face_detector.h"
#include <algorithm>
#include <iostream>

// HSEmotion (EfficientNet-B0) 8 类标签
static const std::vector<std::string> MODEL_LABELS = {
    "Anger", "Contempt", "Disgust", "Fear", "Happiness", "Neutral", "Sadness", "Surprise"
};

// 映射 HSEmotion → 项目内部 Emotion 枚举（Contempt 归入 Neutral）
static const Emotion MODEL_TO_EMOTION[] = {
    Emotion::ANGRY,      // 0: Anger
    Emotion::NEUTRAL,    // 1: Contempt → Neutral
    Emotion::DISGUST,    // 2: Disgust
    Emotion::FEAR,       // 3: Fear
    Emotion::HAPPY,      // 4: Happiness
    Emotion::NEUTRAL,    // 5: Neutral
    Emotion::SAD,        // 6: Sadness
    Emotion::SURPRISED   // 7: Surprise
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
    std::cout << "[EmotionRecognizer] HSEmotion ONNX 模型加载成功" << std::endl;
    return true;
}

Emotion EmotionRecognizer::parseEmotion(const std::string& label) {
    std::string lower = label;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "angry" || lower == "anger")  return Emotion::ANGRY;
    if (lower == "disgust")                    return Emotion::DISGUST;
    if (lower == "fear")                       return Emotion::FEAR;
    if (lower == "happy" || lower == "happiness") return Emotion::HAPPY;
    if (lower == "sad" || lower == "sadness")  return Emotion::SAD;
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

    // 2. 预处理：缩放到 224x224，保持 BGR（HSEmotion 输入尺寸）
    cv::Mat resized;
    cv::resize(face_crop, resized, cv::Size(224, 224));

    // 3. 构造 blob: [1, 3, 224, 224]，ImageNet 归一化
    //    blobFromImage(output) = scale * (input - mean)
    //    先做: (pixel/255 - [0.485,0.456,0.406])
    //    再除以 std=[0.229,0.224,0.225]
    cv::Scalar mean_pixel(0.485 * 255, 0.456 * 255, 0.406 * 255);
    cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0 / 255.0, cv::Size(224, 224), mean_pixel, false);

    // 手动除以 std (blobFromImage 不支持 per-channel std)
    static const float std_vals[] = {0.229f, 0.224f, 0.225f};
    for (int c = 0; c < 3; ++c) {
        cv::Mat channel(224, 224, CV_32F, blob.ptr<float>(0, c));
        channel /= std_vals[c];
    }

    // 4. 前向推理
    net_.setInput(blob);
    cv::Mat output = net_.forward();

    // 5. 解析输出（8 类 logits）→ argmax → 映射到 7 类
    // 输出 shape 可能是 [1,8] 或 [8,1]，统一处理
    int num_classes = output.total();
    float* data = reinterpret_cast<float*>(output.data);

    std::cout << "[DEBUG] raw output:";
    for (int i = 0; i < num_classes && i < 8; ++i) {
        std::cout << " " << MODEL_LABELS[i] << "=" << data[i];
    }
    std::cout << std::endl;

    int max_idx = 0;
    float max_val = data[0];
    for (int i = 1; i < num_classes && i < 8; ++i) {
        if (data[i] > max_val) {
            max_val = data[i];
            max_idx = i;
        }
    }

    // 累加置信度（softmax 后的概率）
    // 先做 softmax 得到概率
    float sum_exp = 0.0f;
    for (int i = 0; i < num_classes && i < 8; ++i) {
        sum_exp += std::exp(data[i]);
    }

    confidences_.assign(7, 0.0f);
    for (int i = 0; i < num_classes && i < 8; ++i) {
        float prob = std::exp(data[i]) / sum_exp;
        int our_idx = static_cast<int>(MODEL_TO_EMOTION[i]);
        confidences_[our_idx] += prob;
    }

    return MODEL_TO_EMOTION[max_idx];
}
