#ifndef EMOTION_RECOGNIZER_H
#define EMOTION_RECOGNIZER_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <unistd.h>
#include <sys/types.h>

// 情绪类型枚举
enum class Emotion {
    ANGRY     = 0,
    DISGUST   = 1,
    FEAR      = 2,
    HAPPY     = 3,
    SAD       = 4,
    SURPRISED = 5,
    NEUTRAL   = 6
};

std::string emotionToString(Emotion e);

class EmotionRecognizer {
public:
    EmotionRecognizer() = default;
    ~EmotionRecognizer();

    // 禁止拷贝
    EmotionRecognizer(const EmotionRecognizer&) = delete;
    EmotionRecognizer& operator=(const EmotionRecognizer&) = delete;

    // 初始化（启动 Python daemon 子进程）
    bool loadModel(const std::string& script_path);

    // 基于人脸图像识别情绪（写临时文件 → 通过管道发送路径 → 读取结果）
    Emotion recognizeFromImage(const cv::Mat& frame, const struct FaceRect& face);

    // 获取各情绪的置信度
    const std::vector<float>& getConfidences() const { return confidences_; }

    static const std::vector<std::string> EMOTION_LABELS;

private:
    std::string script_path_;
    bool initialized_ = false;
    std::vector<float> confidences_;
    int temp_counter_ = 0;

    // daemon 子进程相关
    pid_t daemon_pid_ = -1;
    int pipe_to_child_ = -1;    // 父进程写，子进程读
    int pipe_from_child_ = -1;  // 子进程写，父进程读

    bool startDaemon();
    void stopDaemon();

    Emotion parseEmotion(const std::string& label);
};

#endif // EMOTION_RECOGNIZER_H
