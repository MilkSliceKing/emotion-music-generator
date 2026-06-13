#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <deque>
#include <fstream>
#include <sstream>
#include <map>

#include "detector/face_detector.h"
#include "detector/emotion_recognizer.h"
#include "mapper/emotion_mapper.h"
#include "generator/music_generator.h"
#include "audio/audio_player.h"
#include "audio/local_music_player.h"
#include "ui/overlay_renderer.h"
#include "state/shared_state.h"
#include "web/web_server.h"
#include "logger/emotion_logger.h"
#include "profiler/perf_profiler.h"

// 全局 logger 指针，供 WebServer 访问
EmotionLogger* g_logger = nullptr;

// 简易 YAML 配置读取器（仅支持 key: value 格式）
static std::map<std::string, std::string> loadConfig(const std::string& path) {
    std::map<std::string, std::string> config;
    std::ifstream file(path);
    std::string line, section;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.back() == ':' && line.find(' ') == std::string::npos) {
            section = line.substr(0, line.size() - 1) + ".";
            continue;
        }
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            auto trim = [](std::string& s) {
                size_t start = s.find_first_not_of(" \t");
                size_t end = s.find_last_not_of(" \t");
                if (start == std::string::npos) { s = ""; return; }
                s = s.substr(start, end - start + 1);
                if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                    s = s.substr(1, s.size() - 2);
            };
            trim(key); trim(val);
            config[section + key] = val;
        }
    }
    return config;
}

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

    // ---- 加载配置文件 ----
    auto cfg = loadConfig("config/config.yaml");
    std::string face_model_path = cfg.count("detection.model_path") ? cfg["detection.model_path"] : "models/shape_predictor_68_face_landmarks.dat";
    std::string emotion_model_path = cfg.count("emotion.model_path") ? cfg["emotion.model_path"] : "models/emotion_detector/enet_b0_8_best_afew.onnx";

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

    // ---- 初始化人脸检测器 (dlib HOG) ----
    FaceDetector detector;
    if (!detector.loadModel(face_model_path)) {
        std::cerr << "[错误] 人脸检测模型加载失败: " << face_model_path << std::endl;
        return -1;
    }

    // ---- 初始化情绪识别 (onnxruntime, HSEmotion ONNX) ----
    EmotionRecognizer recognizer;
    if (!recognizer.loadModel(emotion_model_path)) {
        std::cerr << "[错误] 情绪识别模型加载失败: " << emotion_model_path << std::endl;
        return -1;
    }

    // ---- 初始化其他模块 ----
    EmotionMapper mapper;
    MusicGenerator generator;
    AudioPlayer player;
    OverlayRenderer renderer;

    // ---- 初始化新模块（Web / 本地音乐 / 情绪日记）----
    SharedState shared_state;
    LocalMusicPlayer local_player;
    EmotionLogger emotion_logger;
    WebServer web_server;

    // 读取新配置
    bool web_enabled = cfg.count("web.enabled") ? (cfg["web.enabled"] == "true") : true;
    int web_port = cfg.count("web.port") ? std::stoi(cfg["web.port"]) : 8080;
    bool local_music_enabled = cfg.count("local_music.enabled") ? (cfg["local_music.enabled"] == "true") : true;
    std::string music_library_path = cfg.count("local_music.library_path") ? cfg["local_music.library_path"] : "music";
    bool logger_enabled = cfg.count("logger.enabled") ? (cfg["logger.enabled"] == "true") : true;
    std::string logger_data_dir = cfg.count("logger.data_dir") ? cfg["logger.data_dir"] : "data";

    int playback_mode = 0;  // 0 = 合成, 1 = 本地音乐

    if (local_music_enabled) {
        local_player.init(music_library_path);
    }
    if (logger_enabled) {
        emotion_logger.init(logger_data_dir);
        g_logger = &emotion_logger;
    }
    if (web_enabled) {
        web_server.start(web_port, &shared_state);
        shared_state.web_enabled = true;
        shared_state.web_port = web_port;
    }

    // 注册鼠标回调（用于屏幕按钮交互）
    cv::namedWindow("Emotion Music Generator", cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback("Emotion Music Generator", OverlayRenderer::onMouse, &renderer);

    std::cout << "[初始化] 完成!" << std::endl;
    std::cout << "按 ESC 退出, SPACE 手动播放, M 切换自动, L 切换模式, P 性能面板" << std::endl;

    // ---- 主循环 ----
    cv::Mat frame;
    auto last_time = std::chrono::steady_clock::now();
    int fps_counter = 0;
    int total_frames = 0;
    double fps = 0.0;

    std::deque<Emotion> emotion_buffer;
    std::deque<Emotion> emotion_history;  // 情绪历史（最近200帧）
    Emotion current_emotion = Emotion::NEUTRAL;
    auto last_music_time = std::chrono::steady_clock::now();
    bool music_enabled = true;
    MusicParams current_params = mapper.mapToMusic(Emotion::NEUTRAL);

    // 情绪日志节流（不需要每帧都记）
    auto last_log_time = std::chrono::steady_clock::now();
    bool show_perf_panel = false;

    while (true) {
        auto frame_start = std::chrono::high_resolution_clock::now();

        { // CAPTURE 探针
        ScopedTimer timer_capture(Stage::CAPTURE);
        if (is_image_mode) {
            frame = static_image.clone();
        } else {
            cap >> frame;
        }
        if (frame.empty()) {
            std::cerr << "[警告] 空帧，输入结束" << std::endl;
            break;
        }
        } // CAPTURE 探针结束

        // ---- FPS 计算 ----
        fps_counter++;
        total_frames++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
        if (elapsed >= 1000) {
            fps = fps_counter * 1000.0 / elapsed;
            fps_counter = 0;
            last_time = now;
        }

        // ---- 面部检测 ----
        { ScopedTimer timer_detect(Stage::DETECT);
        auto faces = detector.detectFaces(frame);
        } // DETECT 探针结束

        bool face_detected = !faces.empty();
        bool music_just_played = false;

        if (face_detected) {
            // 取最大的人脸
            auto biggest = std::max_element(faces.begin(), faces.end(),
                [](const FaceRect& a, const FaceRect& b) {
                    return a.width * a.height < b.width * b.height;
                });

            // 绘制面部框（使用情绪对应颜色）
            detector.drawFace(frame, *biggest, OverlayRenderer::getEmotionColor(current_emotion));

            // 提取并绘制关键点
            { ScopedTimer timer_landmark(Stage::LANDMARK);
            auto landmarks = detector.getLandmarks(frame, *biggest);
            if (!landmarks.empty()) {
                detector.drawLandmarks(frame, landmarks);
            }
            } // LANDMARK 探针结束

            // ---- 情绪识别 ----
            { ScopedTimer timer_recognize(Stage::RECOGNIZE);
            Emotion detected = recognizer.recognizeFromImage(frame, *biggest);
            emotion_buffer.push_back(detected);
            if (static_cast<int>(emotion_buffer.size()) > SMOOTH_WINDOW) {
                emotion_buffer.pop_front();
            }
            current_emotion = smoothEmotion(emotion_buffer);
            current_params = mapper.mapToMusic(current_emotion);
            } // RECOGNIZE+SMOOTH 探针结束

            // 更新情绪历史
            emotion_history.push_back(current_emotion);
            if (emotion_history.size() > 200) emotion_history.pop_front();

            // 自动播放音乐 (每5秒，图片模式立即播放)
            { ScopedTimer timer_music(Stage::MUSIC_GEN);
            if (music_enabled) {
                auto time_since_music = std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_music_time).count();
                if (is_image_mode || time_since_music >= 5) {
                    if (playback_mode == 0) {
                        auto composition = generator.generateComposition(current_params);
                        player.playComposition(composition, current_params.mood);
                    } else if (local_music_enabled) {
                        local_player.playForEmotion(current_emotion);
                    }
                    last_music_time = now;
                    music_just_played = true;
                    std::cout << "[播放] " << emotionToString(current_emotion)
                              << " -> " << current_params.key << " " << current_params.scale
                              << " " << current_params.tempo << "BPM"
                              << " (" << (playback_mode == 0 ? "合成" : "本地") << ")" << std::endl;
                }
            }
            } // MUSIC_GEN 探针结束

            // 情绪日志（每 3 秒记录一次，避免日志膨胀）
            if (logger_enabled) {
                auto time_since_log = std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_log_time).count();
                if (time_since_log >= 3) {
                    emotion_logger.log(current_emotion, recognizer.getConfidences(),
                                       music_just_played, playback_mode == 0 ? "synth" : "local");
                    last_log_time = now;
                }
            }
        }

        // ---- 绘制 UI overlay ----
        ButtonAction btn_action = ButtonAction::NONE;
        { ScopedTimer timer_ui(Stage::UI_RENDER);
        btn_action = renderer.render(frame, fps, current_emotion,
                        recognizer.getConfidences(), current_params,
                        player.isPlaying() || local_player.isPlaying(), music_enabled,
                        face_detected, total_frames, emotion_history);
        } // UI_RENDER 探针结束

        // ---- 更新共享状态（供 Web 线程读取）----
        { ScopedTimer timer_state(Stage::STATE_UPDATE);
        if (web_enabled) {
            shared_state.setFrame(frame);
            shared_state.setEmotionData(current_emotion, recognizer.getConfidences(),
                                        current_params, face_detected, fps, total_frames);
            shared_state.is_playing = player.isPlaying() || local_player.isPlaying();
            shared_state.music_enabled = music_enabled;
            shared_state.playback_mode = playback_mode;
        }
        } // STATE_UPDATE 探针结束

        // ---- 性能数据写入共享状态 ----
        {
            std::array<StageTiming, STAGE_COUNT> snap;
            PerfProfiler::instance().snapshot(snap);
            auto frame_end = std::chrono::high_resolution_clock::now();
            double total_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
            shared_state.setPerfData(snap, total_ms);
        }

        cv::imshow("Emotion Music Generator", frame);

        // 处理按钮点击
        if (btn_action == ButtonAction::QUIT) break;

        if (btn_action == ButtonAction::PLAY) {
            if (playback_mode == 0) {
                auto composition = generator.generateComposition(current_params);
                player.playComposition(composition, current_params.mood);
            } else if (local_music_enabled) {
                local_player.playForEmotion(current_emotion);
            }
            std::cout << "[按钮播放] " << emotionToString(current_emotion)
                      << " (" << (playback_mode == 0 ? "合成" : "本地") << ")" << std::endl;
        }

        if (btn_action == ButtonAction::AUTO_TOGGLE) {
            music_enabled = !music_enabled;
            std::cout << "[自动播放] " << (music_enabled ? "开启" : "关闭") << std::endl;
        }

        // 键盘控制
        int key = cv::waitKey(is_image_mode ? 0 : 1);
        if (key == 27) break;

        if (key == ' ') {
            if (playback_mode == 0) {
                auto composition = generator.generateComposition(current_params);
                player.playComposition(composition, current_params.mood);
            } else if (local_music_enabled) {
                local_player.playForEmotion(current_emotion);
            }
            std::cout << "[手动播放] " << emotionToString(current_emotion) << std::endl;
        }

        if (key == 'm' || key == 'M') {
            music_enabled = !music_enabled;
            std::cout << "[自动播放] " << (music_enabled ? "开启" : "关闭") << std::endl;
        }

        if (key == 'l' || key == 'L') {
            playback_mode = 1 - playback_mode;
            std::cout << "[模式] 切换到 " << (playback_mode == 0 ? "合成" : "本地音乐") << std::endl;
        }

        if (key == 'p' || key == 'P') {
            show_perf_panel = !show_perf_panel;
            renderer.setShowPerfPanel(show_perf_panel);
            std::cout << "[性能面板] " << (show_perf_panel ? "显示" : "隐藏") << std::endl;
        }

        // ---- 处理 Web 控制请求 ----
        if (shared_state.play_requested.exchange(false)) {
            if (playback_mode == 0) {
                auto composition = generator.generateComposition(current_params);
                player.playComposition(composition, current_params.mood);
            } else if (local_music_enabled) {
                local_player.playForEmotion(current_emotion);
            }
            std::cout << "[Web播放] " << emotionToString(current_emotion) << std::endl;
        }
        if (shared_state.auto_toggle_requested.exchange(false)) {
            music_enabled = !music_enabled;
            std::cout << "[Web] 自动播放 " << (music_enabled ? "开启" : "关闭") << std::endl;
        }
        if (shared_state.mode_switch_requested.exchange(false)) {
            playback_mode = 1 - playback_mode;
            std::cout << "[Web] 切换到 " << (playback_mode == 0 ? "合成" : "本地音乐") << std::endl;
        }
        if (shared_state.pause_requested.exchange(false)) {
            player.stop();
            local_player.stop();
            std::cout << "[Web] 停止播放" << std::endl;
        }

        if (is_image_mode && key >= 0) break;
    }

    // ---- 清理 ----
    if (web_enabled) web_server.stop();
    cap.release();
    cv::destroyAllWindows();
    player.cleanup();
    local_player.cleanup();
    std::cout << "程序已退出" << std::endl;

    return 0;
}
