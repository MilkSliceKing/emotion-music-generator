#ifndef EMOTION_LOGGER_H
#define EMOTION_LOGGER_H

#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include "detector/emotion_recognizer.h"

// 情绪日记模块 — 按天记录情绪变化，生成日报/周报
class EmotionLogger {
public:
    EmotionLogger() = default;

    // 初始化：创建数据目录
    bool init(const std::string& data_dir);

    // 记录一次情绪检测
    void log(Emotion emotion, const std::vector<float>& confidences,
             bool music_played, const std::string& mode);

    // 获取指定日期的原始 CSV 日志
    std::string getDayLog(const std::string& date) const;

    // 获取指定日期的日报摘要（JSON）
    std::string getDailySummary(const std::string& date) const;

    // 获取最近 7 天的周报摘要（JSON）
    std::string getWeeklySummary() const;

    // 获取今天的日期字符串 YYYY-MM-DD
    static std::string todayString();

private:
    std::string data_dir_;
    std::string current_date_;
    std::ofstream current_file_;
    mutable std::mutex log_mutex_;

    void ensureFileOpen();
    std::string buildCsvPath(const std::string& date) const;

    // 解析 CSV 文件，统计各情绪出现次数
    struct DayStats {
        int total = 0;
        int counts[7] = {0};
        int music_count = 0;
        int transitions = 0;
        std::string last_emotion;
    };
    DayStats parseDayCsv(const std::string& date) const;
};

#endif // EMOTION_LOGGER_H
