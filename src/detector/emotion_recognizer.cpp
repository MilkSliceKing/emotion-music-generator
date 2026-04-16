#include "emotion_recognizer.h"
#include "face_detector.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <sys/wait.h>

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

// ---- daemon 子进程管理 ----

bool EmotionRecognizer::startDaemon() {
    int to_child[2];    // parent writes, child reads
    int from_child[2];  // child writes, parent reads

    if (pipe(to_child) == -1 || pipe(from_child) == -1) {
        std::cerr << "[EmotionRecognizer] pipe() 失败" << std::endl;
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "[EmotionRecognizer] fork() 失败" << std::endl;
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        return false;
    }

    if (pid == 0) {
        // ---- 子进程 ----
        close(to_child[1]);     // 关闭写端
        close(from_child[0]);   // 关闭读端

        // 重定向 stdin → to_child[0]
        dup2(to_child[0], STDIN_FILENO);
        close(to_child[0]);

        // 重定向 stdout → from_child[1]
        dup2(from_child[1], STDOUT_FILENO);
        close(from_child[1]);

        // 重定向 stderr → /dev/null
        FILE* devnull = fopen("/dev/null", "w");
        if (devnull) {
            dup2(fileno(devnull), STDERR_FILENO);
            fclose(devnull);
        }

        // exec Python daemon（无参数 = daemon 模式）
        execlp("python3", "python3", script_path_.c_str(), nullptr);

        // 如果 exec 失败
        _exit(127);
    }

    // ---- 父进程 ----
    close(to_child[0]);     // 关闭读端
    close(from_child[1]);   // 关闭写端

    pipe_to_child_ = to_child[1];
    pipe_from_child_ = from_child[0];
    daemon_pid_ = pid;

    // 等待子进程输出 "READY"
    char buf[256];
    std::string ready;
    while (true) {
        ssize_t n = read(pipe_from_child_, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        ready += buf;
        if (ready.find("READY") != std::string::npos) break;
    }

    if (ready.find("READY") == std::string::npos) {
        std::cerr << "[EmotionRecognizer] Python daemon 未能就绪" << std::endl;
        stopDaemon();
        return false;
    }

    std::cout << "[EmotionRecognizer] Python daemon 已启动 (PID=" << pid << ")" << std::endl;
    return true;
}

void EmotionRecognizer::stopDaemon() {
    if (daemon_pid_ > 0) {
        // 发送 EXIT 命令
        if (pipe_to_child_ >= 0) {
            const char* cmd = "EXIT\n";
            write(pipe_to_child_, cmd, strlen(cmd));
            close(pipe_to_child_);
            pipe_to_child_ = -1;
        }
        if (pipe_from_child_ >= 0) {
            close(pipe_from_child_);
            pipe_from_child_ = -1;
        }
        // 等待子进程退出（最多 2 秒）
        int status;
        pid_t ret = waitpid(daemon_pid_, &status, WNOHANG);
        if (ret == 0) {
            // 子进程还在运行，给一点时间
            usleep(100000); // 100ms
            waitpid(daemon_pid_, &status, WNOHANG);
        }
        std::cout << "[EmotionRecognizer] Python daemon 已停止" << std::endl;
        daemon_pid_ = -1;
    }
}

EmotionRecognizer::~EmotionRecognizer() {
    stopDaemon();
}

// ---- 主接口 ----

bool EmotionRecognizer::loadModel(const std::string& script_path) {
    script_path_ = script_path;

    // 启动 Python daemon 子进程
    if (startDaemon()) {
        initialized_ = true;
        return true;
    }

    std::cerr << "[EmotionRecognizer] daemon 启动失败，回退到单次调用模式" << std::endl;
    initialized_ = true;  // 仍标记为已初始化，回退用 popen
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

    // 3. 通过管道发送路径给 daemon
    std::string result;

    if (daemon_pid_ > 0 && pipe_to_child_ >= 0) {
        // ---- daemon 模式 ----
        std::string cmd = temp_path + "\n";
        if (write(pipe_to_child_, cmd.c_str(), cmd.size()) <= 0) {
            std::cerr << "[EmotionRecognizer] 写管道失败" << std::endl;
            std::remove(temp_path.c_str());
            return Emotion::NEUTRAL;
        }

        // 读取一行结果
        char buf[512];
        while (true) {
            ssize_t n = read(pipe_from_child_, buf, sizeof(buf) - 1);
            if (n <= 0) break;
            buf[n] = '\0';
            result += buf;
            if (result.find('\n') != std::string::npos) break;
        }
    } else {
        // ---- 回退：单次 popen 模式 ----
        std::string cmd = "python3 " + script_path_ + " " + temp_path + " 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::cerr << "[EmotionRecognizer] 无法执行 Python 脚本" << std::endl;
            std::remove(temp_path.c_str());
            return Emotion::NEUTRAL;
        }
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) {
            result += buf;
        }
        pclose(pipe);
    }

    // 删除临时文件
    std::remove(temp_path.c_str());

    // 4. 解析输出
    if (result.empty() || result.substr(0, 5) == "ERROR") {
        return Emotion::NEUTRAL;
    }

    // 去掉换行
    if (!result.empty() && result.back() == '\n') result.pop_back();

    // daemon 模式输出: OK,dominant,angry,disgust,fear,happy,sad,surprise,neutral
    // 回退模式输出: dominant,angry,disgust,fear,happy,sad,surprise,neutral
    if (result.substr(0, 3) == "OK,") {
        result = result.substr(3);  // 去掉 "OK," 前缀
    }

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
