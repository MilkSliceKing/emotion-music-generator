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

// 最简单的情绪平滑: 取众数
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

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  情绪音乐生成器 (Emotion Music Generator)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "初始化中..." << std::endl;

    // ---- 初始化摄像头 ----
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "[错误] 无法打开摄像头" << std::endl;
        std::cerr << "  1. 检查摄像头是否连接" << std::endl;
        std::cerr << "  2. 检查虚拟机 USB 摄像头映射" << std::endl;
        return -1;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    std::cout << "[摄像头] 已打开 (640x480)" << std::endl;

    // ---- 初始化面部检测器 ----
    FaceDetector detector;
    if (!detector.loadModel("models/shape_predictor_68_face_landmarks.dat")) {
        std::cerr << "[错误] Landmark 模型加载失败，程序退出" << std::endl;
        return -1;
    }

    // ---- 初始化各模块 ----
    EmotionRecognizer recognizer;
    EmotionMapper mapper;
    MusicGenerator generator;
    AudioPlayer player;

    std::cout << "[初始化] 完成! 按 ESC 退出, 按 SPACE 手动触发音乐" << std::endl;

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
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "[警告] 空帧，摄像头可能断开" << std::endl;
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
            // 取最大的人脸 (面积最大的)
            auto biggest = std::max_element(faces.begin(), faces.end(),
                [](const FaceRect& a, const FaceRect& b) {
                    return a.width * a.height < b.width * b.height;
                });

            // 提取关键点
            auto landmarks = detector.getLandmarks(frame, *biggest);

            if (landmarks.size() == 68) {
                // 绘制面部框和关键点
                detector.drawFace(frame, *biggest);
                detector.drawLandmarks(frame, landmarks);

                // ---- 情绪识别 ----
                Emotion detected = recognizer.recognize(landmarks);
                emotion_buffer.push_back(detected);
                if (static_cast<int>(emotion_buffer.size()) > SMOOTH_WINDOW) {
                    emotion_buffer.pop_front();
                }
                current_emotion = smoothEmotion(emotion_buffer);

                // 显示情绪标签
                std::string emotion_text = "Emotion: " + emotionToString(current_emotion);
                cv::putText(frame, emotion_text,
                            cv::Point(10, 60),
                            cv::FONT_HERSHEY_SIMPLEX, 0.7,
                            cv::Scalar(255, 200, 0), 2);

                // 显示音乐参数
                auto params = mapper.mapToMusic(current_emotion);
                std::string music_text = "Music: " + params.key + " " + params.scale +
                                         " " + std::to_string(params.tempo) + "BPM";
                cv::putText(frame, music_text,
                            cv::Point(10, 90),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5,
                            cv::Scalar(200, 200, 200), 1);

                // ---- 自动播放音乐 (每5秒) ----
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
            }
        } else {
            // 没检测到人脸
            cv::putText(frame, "No face detected",
                        cv::Point(10, 60),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7,
                        cv::Scalar(0, 0, 255), 2);
        }

        // ---- 显示 FPS ----
        cv::putText(frame, "FPS: " + std::to_string(static_cast<int>(fps)),
                    cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    cv::Scalar(0, 255, 0), 2);

        // 显示控制提示
        cv::putText(frame, "ESC:Quit  SPACE:Play  M:ToggleMusic",
                    cv::Point(10, frame.rows - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    cv::Scalar(180, 180, 180), 1);

        cv::imshow("Emotion Music Generator", frame);

        // ---- 键盘控制 ----
        int key = cv::waitKey(1);
        if (key == 27) break;  // ESC 退出

        if (key == ' ') {      // 空格 手动播放
            auto params = mapper.mapToMusic(current_emotion);
            auto notes = generator.generate(params);
            player.play(notes);
            std::cout << "[手动播放] " << emotionToString(current_emotion) << std::endl;
        }

        if (key == 'm' || key == 'M') {  // M 切换自动播放
            music_enabled = !music_enabled;
            std::cout << "[自动播放] " << (music_enabled ? "开启" : "关闭") << std::endl;
        }
    }

    // ---- 清理 ----
    cap.release();
    cv::destroyAllWindows();
    player.cleanup();
    std::cout << "程序已退出" << std::endl;

    return 0;
}
