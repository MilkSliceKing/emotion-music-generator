#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include <opencv2/opencv.hpp>
#include "detector/face_detector.h"
#include "detector/emotion_recognizer.h"

namespace fs = std::filesystem;

// 文件夹名 → Emotion 枚举（与主程序映射一致）
static Emotion labelToEmotion(const std::string& label) {
    std::string lower = label;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "angry" || lower == "anger")     return Emotion::ANGRY;
    if (lower == "disgust")                        return Emotion::DISGUST;
    if (lower == "fear")                           return Emotion::FEAR;
    if (lower == "happy" || lower == "happiness")  return Emotion::HAPPY;
    if (lower == "sad" || lower == "sadness")      return Emotion::SAD;
    if (lower == "surprised" || lower == "surprise") return Emotion::SURPRISED;
    if (lower == "neutral")                        return Emotion::NEUTRAL;
    return Emotion::NEUTRAL; // fallback
}

// 是否为图片扩展名
static bool isImageFile(const std::string& ext) {
    std::string lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == ".jpg" || lower == ".jpeg" || lower == ".png" || lower == ".bmp";
}

// 单张图片的测试结果
struct TestResult {
    std::string filename;
    std::string true_label;
    std::string predicted_label;
    bool correct;
    std::vector<float> confidences; // 7 类概率
    bool face_found;
    std::string error_msg;
};

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  表情识别测试工具 (Emotion Test Tool)" << std::endl;
    std::cout << "========================================" << std::endl;

    if (argc < 2) {
        std::cout << "用法:" << std::endl;
        std::cout << "  " << argv[0] << " <测试图片根目录> [输出CSV路径]" << std::endl;
        std::cout << std::endl;
        std::cout << "图片根目录下的子文件夹名对应情绪标签:" << std::endl;
        std::cout << "  angry/ disgust/ fear/ happy/ sad/ surprised/ neutral/" << std::endl;
        return 0;
    }

    fs::path test_dir(argv[1]);
    if (!fs::exists(test_dir) || !fs::is_directory(test_dir)) {
        std::cerr << "[错误] 目录不存在: " << test_dir << std::endl;
        return -1;
    }

    // 输出 CSV 路径
    fs::path csv_path = (argc >= 3) ? fs::path(argv[2]) : test_dir / "test_results.csv";

    // 初始化模型
    std::cout << "加载模型..." << std::endl;

    FaceDetector detector;
    if (!detector.loadModel("models/shape_predictor_68_face_landmarks.dat")) {
        std::cerr << "[错误] 人脸检测模型加载失败" << std::endl;
        return -1;
    }

    EmotionRecognizer recognizer;
    if (!recognizer.loadModel("models/emotion_detector/enet_b0_8_best_afew.onnx")) {
        std::cerr << "[错误] 情绪识别模型加载失败" << std::endl;
        return -1;
    }

    std::cout << "模型加载完成" << std::endl;
    std::cout << "扫描测试图片..." << std::endl;

    // 收集所有测试结果
    std::vector<TestResult> results;

    // 混淆矩阵 [真实][预测]
    int confusion[7][7] = {};

    // 遍历子文件夹
    for (const auto& entry : fs::directory_iterator(test_dir)) {
        if (!entry.is_directory()) continue;

        std::string folder_name = entry.path().filename().string();
        Emotion true_emotion = labelToEmotion(folder_name);
        int true_idx = static_cast<int>(true_emotion);

        std::cout << "处理 " << folder_name << "/ ..." << std::endl;

        int folder_count = 0;
        int folder_no_face = 0;

        for (const auto& img_entry : fs::directory_iterator(entry.path())) {
            if (!img_entry.is_regular_file()) continue;
            if (!isImageFile(img_entry.path().extension().string())) continue;

            folder_count++;
            std::string rel_path = fs::relative(img_entry.path(), test_dir).string();

            cv::Mat frame = cv::imread(img_entry.path().string());
            if (frame.empty()) {
                results.push_back({rel_path, folder_name, "", false, {}, false, "无法读取图片"});
                continue;
            }

            // 检测人脸
            auto faces = detector.detectFaces(frame);
            if (faces.empty()) {
                results.push_back({rel_path, folder_name, "", false, {}, false, "未检测到人脸"});
                folder_no_face++;
                continue;
            }

            // 取最大人脸
            auto biggest = std::max_element(faces.begin(), faces.end(),
                [](const FaceRect& a, const FaceRect& b) {
                    return a.width * a.height < b.width * b.height;
                });

            // 情绪识别
            Emotion predicted = recognizer.recognizeFromImage(frame, *biggest);
            const auto& confs = recognizer.getConfidences();
            int pred_idx = static_cast<int>(predicted);

            TestResult r;
            r.filename = rel_path;
            r.true_label = folder_name;
            r.predicted_label = emotionToString(predicted);
            r.correct = (predicted == true_emotion);
            r.confidences = confs;
            r.face_found = true;

            results.push_back(r);
            confusion[true_idx][pred_idx]++;
        }

        std::cout << "  " << folder_count << " 张图片";
        if (folder_no_face > 0) {
            std::cout << " (" << folder_no_face << " 张未检测到人脸)";
        }
        std::cout << std::endl;
    }

    // 写入 CSV
    std::ofstream csv(csv_path);
    if (!csv.is_open()) {
        std::cerr << "[错误] 无法创建 CSV: " << csv_path << std::endl;
        return -1;
    }

    // CSV 表头
    csv << "filename,true_label,predicted_label,correct,face_found";
    for (const auto& label : EmotionRecognizer::EMOTION_LABELS) {
        csv << "," << label;
    }
    csv << ",error" << std::endl;

    // CSV 数据行
    for (const auto& r : results) {
        csv << r.filename << ","
            << r.true_label << ","
            << r.predicted_label << ","
            << (r.correct ? "1" : "0") << ","
            << (r.face_found ? "1" : "0");

        if (!r.confidences.empty()) {
            for (float c : r.confidences) {
                csv << "," << std::fixed << std::setprecision(4) << c;
            }
        } else {
            for (int i = 0; i < 7; i++) csv << ",";
        }

        csv << "," << r.error_msg << std::endl;
    }

    // 汇总统计
    csv << std::endl;
    csv << "[Summary]" << std::endl;

    int total = 0, correct = 0, no_face = 0;
    for (const auto& r : results) {
        if (!r.face_found) { no_face++; continue; }
        total++;
        if (r.correct) correct++;
    }

    csv << "total_images," << results.size() << std::endl;
    csv << "face_detected," << total << std::endl;
    csv << "no_face," << no_face << std::endl;
    csv << "correct," << correct << std::endl;
    csv << "accuracy," << std::fixed << std::setprecision(2)
        << (total > 0 ? 100.0 * correct / total : 0.0) << "%" << std::endl;

    // 各类准确率
    csv << std::endl;
    csv << "[Per-class accuracy]" << std::endl;
    csv << "class,total,correct,accuracy" << std::endl;
    for (int i = 0; i < 7; i++) {
        int class_total = 0;
        for (int j = 0; j < 7; j++) class_total += confusion[i][j];
        int class_correct = confusion[i][i];
        if (class_total > 0) {
            csv << EmotionRecognizer::EMOTION_LABELS[i] << ","
                << class_total << ","
                << class_correct << ","
                << std::fixed << std::setprecision(2)
                << 100.0 * class_correct / class_total << "%" << std::endl;
        }
    }

    // 混淆矩阵
    csv << std::endl;
    csv << "[Confusion Matrix]" << std::endl;
    csv << "true\\pred";
    for (const auto& label : EmotionRecognizer::EMOTION_LABELS) {
        csv << "," << label;
    }
    csv << std::endl;

    for (int i = 0; i < 7; i++) {
        csv << EmotionRecognizer::EMOTION_LABELS[i];
        for (int j = 0; j < 7; j++) {
            csv << "," << confusion[i][j];
        }
        csv << std::endl;
    }

    csv.close();

    // 终端输出汇总
    std::cout << std::endl;
    std::cout << "========== 测试结果 ==========" << std::endl;
    std::cout << "总图片数:   " << results.size() << std::endl;
    std::cout << "检测到人脸: " << total << std::endl;
    std::cout << "未检测到:   " << no_face << std::endl;
    std::cout << "预测正确:   " << correct << std::endl;
    std::cout << "总体准确率: " << std::fixed << std::setprecision(1)
              << (total > 0 ? 100.0 * correct / total : 0.0) << "%" << std::endl;

    std::cout << std::endl << "--- 各类准确率 ---" << std::endl;
    for (int i = 0; i < 7; i++) {
        int class_total = 0;
        for (int j = 0; j < 7; j++) class_total += confusion[i][j];
        if (class_total > 0) {
            std::cout << "  " << std::left << std::setw(10) << EmotionRecognizer::EMOTION_LABELS[i]
                      << std::right << std::setw(3) << confusion[i][i] << "/" << class_total
                      << "  " << std::fixed << std::setprecision(1)
                      << 100.0 * confusion[i][i] / class_total << "%" << std::endl;
        }
    }

    std::cout << std::endl << "--- 混淆矩阵 ---" << std::endl;
    // 表头
    std::cout << std::left << std::setw(10) << "true\\pred";
    for (const auto& label : EmotionRecognizer::EMOTION_LABELS) {
        std::cout << std::right << std::setw(8) << label.substr(0, 7);
    }
    std::cout << std::endl;

    for (int i = 0; i < 7; i++) {
        std::cout << std::left << std::setw(10) << EmotionRecognizer::EMOTION_LABELS[i];
        for (int j = 0; j < 7; j++) {
            std::cout << std::right << std::setw(8) << confusion[i][j];
        }
        std::cout << std::endl;
    }

    std::cout << std::endl << "CSV 已保存到: " << csv_path << std::endl;

    return 0;
}
