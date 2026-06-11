#ifndef LOCAL_MUSIC_PLAYER_H
#define LOCAL_MUSIC_PLAYER_H

#include <string>
#include <vector>
#include <map>
#include "detector/emotion_recognizer.h"

// 本地音乐播放器 — 根据情绪播放本地 MP3/音频文件
class LocalMusicPlayer {
public:
    LocalMusicPlayer() = default;
    ~LocalMusicPlayer() = default;

    // 初始化：扫描音乐库目录
    bool init(const std::string& library_path);

    // 根据情绪播放匹配的音乐文件
    void playForEmotion(Emotion emotion);

    // 停止播放
    void stop();

    // 清理子进程
    void cleanup();

    // 是否正在播放
    bool isPlaying();

    // 获取音乐库统计信息
    std::string getLibraryInfo() const;

private:
    std::string library_path_;
    pid_t child_pid_ = -1;
    bool initialized_ = false;

    // mood名 -> 文件列表缓存
    std::map<std::string, std::vector<std::string>> mood_songs_;

    // 情绪 → 文件夹名映射
    static std::string emotionToMoodFolder(Emotion e);

    // 扫描单个 mood 文件夹
    std::vector<std::string> scanMoodFolder(const std::string& folder_path);

    // 查找可用的播放器命令 (mpv / ffplay / cvlc)
    static std::string findPlayerCommand();
};

#endif // LOCAL_MUSIC_PLAYER_H
