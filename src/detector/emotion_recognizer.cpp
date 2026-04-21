#include "emotion_recognizer.h"
#include "face_detector.h"
#include <algorithm>
#include <iostream>
#include <numeric>

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

EmotionRecognizer::EmotionRecognizer() : env_(ORT_LOGGING_LEVEL_WARNING, "emotion") {
    session_opts_.SetIntraOpNumThreads(4);
    session_opts_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

bool EmotionRecognizer::loadModel(const std::string& model_path) {
    try {
        session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), session_opts_);
        initialized_ = true;
        std::cout << "[EmotionRecognizer] onnxruntime 模型加载成功" << std::endl;
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "[EmotionRecognizer] 模型加载失败: " << e.what() << std::endl;
        return false;
    }
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

    // 2. 预处理：缩放到 224x224 BGR → ImageNet 归一化 → NCHW
    cv::Mat resized;
    cv::resize(face_crop, resized, cv::Size(224, 224));

    resized.convertTo(resized, CV_32F, 1.0 / 255.0);

    // ImageNet 归一化 (BGR 通道顺序，与 HSEmotion Python 代码一致)
    static const float mean[] = {0.485f, 0.456f, 0.406f};
    static const float std_v[] = {0.229f, 0.224f, 0.225f};
    std::vector<cv::Mat> channels(3);
    cv::split(resized, channels);
    for (int c = 0; c < 3; ++c) {
        channels[c] = (channels[c] - mean[c]) / std_v[c];
    }

    // 3. 构造输入 tensor [1, 3, 224, 224] (NCHW)
    std::vector<float> input_data(3 * 224 * 224);
    for (int c = 0; c < 3; ++c) {
        std::memcpy(input_data.data() + c * 224 * 224,
                    channels[c].data, 224 * 224 * sizeof(float));
    }

    std::array<int64_t, 4> input_shape = {1, 3, 224, 224};
    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        mem_info, input_data.data(), input_data.size(), input_shape.data(), input_shape.size());

    // 4. 推理
    const char* input_names[] = {"input"};
    const char* output_names[] = {"output"};
    auto output_tensors = session_->Run(
        Ort::RunOptions{nullptr}, input_names, &input_tensor, 1, output_names, 1);

    // 5. 解析输出（8 类 logits）
    float* output_data = output_tensors[0].GetTensorMutableData<float>();
    auto output_info = output_tensors[0].GetTensorTypeAndShapeInfo();
    size_t num_classes = output_info.GetElementCount();

    // 打印原始 logits 用于调试
    std::cout << "[DEBUG] raw output:";
    for (size_t i = 0; i < num_classes && i < 8; ++i) {
        std::cout << " " << MODEL_LABELS[i] << "=" << output_data[i];
    }
    std::cout << std::endl;

    // 6. Softmax → 概率
    float max_logit = *std::max_element(output_data, output_data + num_classes);
    std::vector<float> probs(num_classes);
    float sum_exp = 0.0f;
    for (size_t i = 0; i < num_classes; ++i) {
        probs[i] = std::exp(output_data[i] - max_logit);
        sum_exp += probs[i];
    }
    for (auto& p : probs) p /= sum_exp;

    // 7. 累加到 7 类置信度
    confidences_.assign(7, 0.0f);
    int max_idx = 0;
    float max_prob = 0.0f;
    for (size_t i = 0; i < num_classes && i < 8; ++i) {
        int our_idx = static_cast<int>(MODEL_TO_EMOTION[i]);
        confidences_[our_idx] += probs[i];
        if (probs[i] > max_prob) {
            max_prob = probs[i];
            max_idx = static_cast<int>(i);
        }
    }

    return MODEL_TO_EMOTION[max_idx];
}
