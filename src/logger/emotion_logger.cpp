#include "emotion_logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sys/stat.h>

static const char* EMOTION_NAMES[] = {
    "Angry", "Disgust", "Fear", "Happy", "Sad", "Surprised", "Neutral"
};

bool EmotionLogger::init(const std::string& data_dir) {
    data_dir_ = data_dir;
    // 创建数据目录
    mkdir(data_dir_.c_str(), 0755);
    std::cout << "[EmotionLogger] 初始化完成, 数据目录: " << data_dir_ << std::endl;
    return true;
}

std::string EmotionLogger::todayString() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time_t_now, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d");
    return oss.str();
}

void EmotionLogger::ensureFileOpen() {
    std::string today = todayString();
    if (current_date_ != today || !current_file_.is_open()) {
        if (current_file_.is_open()) {
            current_file_.close();
        }
        current_date_ = today;
        std::string path = buildCsvPath(today);
        current_file_.open(path, std::ios::app);
        if (current_file_.is_open() && current_file_.tellp() == 0) {
            // 写入 CSV 表头
            current_file_ << "timestamp,emotion,angry_conf,disgust_conf,fear_conf,"
                          << "happy_conf,sad_conf,surprised_conf,neutral_conf,"
                          << "music_played,mode\n";
        }
    }
}

void EmotionLogger::log(Emotion emotion, const std::vector<float>& confidences,
                        bool music_played, const std::string& mode) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    ensureFileOpen();
    if (!current_file_.is_open()) return;

    // 时间戳
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time_t_now, &tm_buf);

    current_file_ << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S") << ",";
    current_file_ << emotionToString(emotion) << ",";

    // 7 个置信度
    for (int i = 0; i < 7; ++i) {
        if (i < static_cast<int>(confidences.size())) {
            current_file_ << std::fixed << std::setprecision(4) << confidences[i];
        } else {
            current_file_ << "0.0000";
        }
        current_file_ << ",";
    }

    current_file_ << (music_played ? "1" : "0") << ",";
    current_file_ << mode << "\n";
    current_file_.flush();
}

std::string EmotionLogger::buildCsvPath(const std::string& date) const {
    return data_dir_ + "/emotion_log_" + date + ".csv";
}

std::string EmotionLogger::getDayLog(const std::string& date) const {
    std::string path = buildCsvPath(date);
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

EmotionLogger::DayStats EmotionLogger::parseDayCsv(const std::string& date) const {
    DayStats stats;
    std::string path = buildCsvPath(date);
    std::ifstream file(path);
    if (!file.is_open()) return stats;

    std::string line;
    // 跳过表头
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        // 解析: timestamp,emotion,conf1,...,conf7,music_played,mode
        std::istringstream iss(line);
        std::string timestamp, emotion_str;
        std::getline(iss, timestamp, ',');
        std::getline(iss, emotion_str, ',');

        // 统计情绪计数
        for (int i = 0; i < 7; ++i) {
            if (emotion_str == EMOTION_NAMES[i]) {
                stats.counts[i]++;
                break;
            }
        }
        stats.total++;

        // 统计情绪转换次数
        if (!stats.last_emotion.empty() && stats.last_emotion != emotion_str) {
            stats.transitions++;
        }
        stats.last_emotion = emotion_str;

        // 统计播放次数（倒数第二个字段）
        // 从行尾往前找
        size_t last_comma = line.rfind(',');
        if (last_comma != std::string::npos) {
            size_t prev_comma = line.rfind(',', last_comma - 1);
            if (prev_comma != std::string::npos) {
                std::string played = line.substr(prev_comma + 1, last_comma - prev_comma - 1);
                if (played == "1") stats.music_count++;
            }
        }
    }
    return stats;
}

std::string EmotionLogger::getDailySummary(const std::string& date) const {
    DayStats stats = parseDayCsv(date);

    std::ostringstream json;
    json << "{";
    json << "\"date\":\"" << date << "\",";
    json << "\"total_detections\":" << stats.total << ",";

    // 找主导情绪
    int max_idx = 0;
    for (int i = 1; i < 7; ++i) {
        if (stats.counts[i] > stats.counts[max_idx]) max_idx = i;
    }
    json << "\"dominant_emotion\":\"" << EMOTION_NAMES[max_idx] << "\",";

    // 分布
    json << "\"distribution\":{";
    for (int i = 0; i < 7; ++i) {
        if (i > 0) json << ",";
        json << "\"" << EMOTION_NAMES[i] << "\":" << stats.counts[i];
    }
    json << "},";

    // 百分比
    json << "\"percentages\":{";
    for (int i = 0; i < 7; ++i) {
        if (i > 0) json << ",";
        double pct = stats.total > 0 ? (100.0 * stats.counts[i] / stats.total) : 0.0;
        json << "\"" << EMOTION_NAMES[i] << "\":" << std::fixed << std::setprecision(1) << pct;
    }
    json << "},";

    json << "\"transitions\":" << stats.transitions << ",";
    json << "\"music_triggered_count\":" << stats.music_count;
    json << "}";

    return json.str();
}

std::string EmotionLogger::getWeeklySummary() const {
    std::string today = todayString();
    std::tm tm_buf = {};

    // 解析今天日期
    std::istringstream iss(today);
    iss >> std::get_time(&tm_buf, "%Y-%m-%d");

    std::ostringstream json;
    json << "{\"days\":[";

    bool first = true;
    for (int d = 6; d >= 0; --d) {
        std::tm target_tm = tm_buf;
        target_tm.tm_mday -= d;
        std::mktime(&target_tm);  // 规范化

        char date_buf[11];
        std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &target_tm);
        std::string date_str(date_buf);

        DayStats stats = parseDayCsv(date_str);

        if (!first) json << ",";
        first = false;

        json << "{";
        json << "\"date\":\"" << date_str << "\",";
        json << "\"total\":" << stats.total << ",";

        int max_idx = 0;
        for (int i = 1; i < 7; ++i) {
            if (stats.counts[i] > stats.counts[max_idx]) max_idx = i;
        }
        json << "\"dominant\":\"" << EMOTION_NAMES[max_idx] << "\",";

        json << "\"distribution\":{";
        for (int i = 0; i < 7; ++i) {
            if (i > 0) json << ",";
            json << "\"" << EMOTION_NAMES[i] << "\":" << stats.counts[i];
        }
        json << "}";
        json << "}";
    }

    json << "]}";
    return json.str();
}
