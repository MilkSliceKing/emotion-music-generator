#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <deque>

#include "detector/face_detector.h"
#include "detector/emotion_recognizer.h"
#include "mapper/emotion_mapper.h"
#include "generator/music_generator.h"
#include "audio/audio_player.h"

// 情绪平滑缓冲区大小
static constexpr int SMOOTH_WINDOW = 5;

// 取众数平滑
static Emotion smoothEmotion(const std::deque<Emotion>& buffer) {
    int counts[7] = {0};
    for (const auto& e : buffer) {
        counts[static_cast<int>(e)]++;
    }
    int max_idx = 0;
    for (int i = 1; i < 7; ++i) {
        if (counts[i] > counts[max_idx]) max_idx = i;
    }
    return static_cast<Emotion>(max_idx);
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  情绪音乐生成器 (Emotion Music Generator)" << std::endl;
    std::cout << "========================================" << std::endl;

    if (argc < 2) {
        std::cout << "用法:" << std::endl;
        std::cout << "  " << argv[0] << " camera        # 使用摄像头" << std::endl;
        std::cout << "  " << argv[0] << " image 图片路径  # 分析单张图片" << std::endl;
        std::cout << "  " << argv[0] << " video 视频路径  # 分析视频文件" << std::endl;
        return 0;
    }

    std::string mode = argv[1];
    std::cout << "初始化中..." << std::endl;

    // ---- 初始化输入源 ----
    cv::VideoCapture cap;
    cv::Mat static_image;
    bool is_image_mode = false;

    if (mode == "camera") {
        cap.open(0);
        if (!cap.isOpened()) {
            std::cerr << "[错误] 无法打开摄像头" << std::endl;
            return -1;
        }
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        std::cout << "[摄像头] 已打开 (640x480)" << std::endl;
    } else if (mode == "image" && argc >= 3) {
        static_image = cv::imread(argv[2]);
        if (static_image.empty()) {
            std::cerr << "[错误] 无法读取图片: " << argv[2] << std::endl;
            return -1;
        }
        is_image_mode = true;
        std::cout << "[图片] 已加载: " << argv[2] << " (" << static_image.cols << "x" << static_image.rows << ")" << std::endl;
    } else if (mode == "video" && argc >= 3) {
        cap.open(argv[2]);
        if (!cap.isOpened()) {
            std::cerr << "[错误] 无法打开视频: " << argv[2] << std::endl;
            return -1;
        }
        std::cout << "[视频] 已打开: " << argv[2] << std::endl;
    } else {
        std::cerr << "[错误] 未知模式: " << mode << std::endl;
        return -1;
    }

    // ---- 初始化人脸检测器 (OpenCV DNN SSD) ----
    FaceDetector detector;
    detector.setConfidenceThreshold(0.3f);
    if (!detector.loadDetector("models/face_detector")) {
        std::cerr << "[错误] 人脸检测模型加载失败" << std::endl;
        return -1;
    }
    // landmark 模型可选（用于可视化关键点）
    detector.loadLandmarkModel("models/shape_predictor_68_face_landmarks.dat");

    // ---- 初始化情绪识别 (FER+ ONNX) ----
    EmotionRecognizer recognizer;
    if (!recognizer.loadModel("models/emotion-ferplus.onnx")) {
        std::cerr << "[错误] 情绪识别模型加载失败" << std::endl;
        return -1;
    }

    // ---- 初始化其他模块 ----
    EmotionMapper mapper;
    MusicGenerator generator;
    AudioPlayer player;

    std::cout << "[初始化] 完成!" << std::endl;
    if (!is_image_mode) {
        std::cout << "按 ESC 退出, 按 SPACE 手动触发音乐, M 切换自动播放" << std::endl;
    }

    // ---- 主循环 ----
    cv::Mat frame;
    auto last_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    double fps = 0.0;

    std::deque<Emotion> emotion_buffer;
    Emotion current_emotion = Emotion::NEUTRAL;
    auto last_music_time = std::chrono::steady_clock::now();
    bool music_enabled = true;

    while (true) {
        if (is_image_mode) {
            frame = static_image.clone();
        } else {
            cap >> frame;
        }
        if (frame.empty()) {
            std::cerr << "[警告] 空帧，输入结束" << std::endl;
            break;
        }

        // ---- FPS 计算 ----
        frame_count++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
        if (elapsed >= 1000) {
            fps = frame_count * 1000.0 / elapsed;
            frame_count = 0;
            last_time = now;
        }

        // ---- 面部检测 ----
        auto faces = detector.detectFaces(frame);

        if (!faces.empty()) {
            // 取最大的人脸
            auto biggest = std::max_element(faces.begin(), faces.end(),
                [](const FaceRect& a, const FaceRect& b) {
                    return a.width * a.height < b.width * b.height;
                });

            // 绘制面部框
            detector.drawFace(frame, *biggest);

            // 提取并绘制关键点（如果 landmark 模型可用）
            auto landmarks = detector.getLandmarks(frame, *biggest);
            if (!landmarks.empty()) {
                detector.drawLandmarks(frame, landmarks);
            }

            // ---- 情绪识别 ----
            Emotion detected = recognizer.recognizeFromImage(frame, *biggest);
            emotion_buffer.push_back(detected);
            if (static_cast<int>(emotion_buffer.size()) > SMOOTH_WINDOW) {
                emotion_buffer.pop_front();
            }
            current_emotion = smoothEmotion(emotion_buffer);

            // 显示情绪标签和置信度
            std::string emotion_text = "Emotion: " + emotionToString(current_emotion);
            cv::putText(frame, emotion_text,
                        cv::Point(10, 60),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7,
                        cv::Scalar(255, 200, 0), 2);

            // 显示 Top3 置信度
            auto& conf = recognizer.getConfidences();
            if (!conf.empty()) {
                std::vector<std::pair<float, int>> sorted;
                for (int i = 0; i < static_cast<int>(conf.size()); ++i) {
                    sorted.push_back({conf[i], i});
                }
                std::sort(sorted.rbegin(), sorted.rend());
                for (int i = 0; i < std::min(3, static_cast<int>(sorted.size())); ++i) {
                    std::string txt = emotionToString(static_cast<Emotion>(sorted[i].second)) +
                                      ": " + std::to_string(sorted[i].first * 100).substr(0, 5) + "%";
                    cv::putText(frame, txt,
                                cv::Point(10, 120 + i * 20),
                                cv::FONT_HERSHEY_SIMPLEX, 0.45,
                                cv::Scalar(180, 180, 180), 1);
                }
            }

            // 显示音乐参数
            auto params = mapper.mapToMusic(current_emotion);
            std::string music_text = "Music: " + params.key + " " + params.scale +
                                     " " + std::to_string(params.tempo) + "BPM";
            cv::putText(frame, music_text,
                        cv::Point(10, 90),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(200, 200, 200), 1);

            // 自动播放音乐 (每5秒)
            if (music_enabled) {
                auto time_since_music = std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_music_time).count();
                if (time_since_music >= 5) {
                    auto notes = generator.generate(params);
                    player.play(notes);
                    last_music_time = now;
                    std::cout << "[播放] " << emotionToString(current_emotion)
                              << " -> " << params.key << " " << params.scale
                              << " " << params.tempo << "BPM" << std::endl;
                }
            }
        } else {
            cv::putText(frame, "No face detected",
                        cv::Point(10, 60),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7,
                        cv::Scalar(0, 0, 255), 2);
        }

        // 显示 FPS
        cv::putText(frame, "FPS: " + std::to_string(static_cast<int>(fps)),
                    cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    cv::Scalar(0, 255, 0), 2);

        cv::imshow("Emotion Music Generator", frame);

        // 键盘控制
        int key = cv::waitKey(is_image_mode ? 0 : 1);
        if (is_image_mode && key >= 0) break;
        if (key == 27) break;

        if (key == ' ') {
            auto params = mapper.mapToMusic(current_emotion);
            auto notes = generator.generate(params);
            player.play(notes);
            std::cout << "[手动播放] " << emotionToString(current_emotion) << std::endl;
        }

        if (key == 'm' || key == 'M') {
            music_enabled = !music_enabled;
            std::cout << "[自动播放] " << (music_enabled ? "开启" : "关闭") << std::endl;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    player.cleanup();
    std::cout << "程序已退出" << std::endl;

    return 0;
}
