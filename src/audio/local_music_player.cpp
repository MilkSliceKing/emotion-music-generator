#include "local_music_player.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static const char* MOOD_FOLDERS[] = {
    "angry", "disgust", "fear", "happy", "sad", "surprised", "neutral"
};

static const char* AUDIO_EXTENSIONS[] = {
    ".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aac", ".wma"
};

static bool hasAudioExtension(const std::string& filename) {
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto* ext : AUDIO_EXTENSIONS) {
        if (lower.size() >= 4 && lower.compare(lower.size() - strlen(ext), strlen(ext), ext) == 0) {
            return true;
        }
    }
    return false;
}

bool LocalMusicPlayer::init(const std::string& library_path) {
    library_path_ = library_path;

    // 检查目录是否存在
    struct stat st;
    if (stat(library_path_.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        std::cout << "[LocalMusicPlayer] 音乐库目录不存在: " << library_path_
                  << "（可稍后创建并添加音乐文件）" << std::endl;
        initialized_ = false;
        return false;
    }

    // 扫描各情绪子目录
    int total_songs = 0;
    for (const auto* mood : MOOD_FOLDERS) {
        std::string folder = library_path_ + "/" + mood;
        auto songs = scanMoodFolder(folder);
        if (!songs.empty()) {
            mood_songs_[mood] = songs;
            total_songs += songs.size();
        }
    }

    initialized_ = true;
    std::cout << "[LocalMusicPlayer] 初始化完成, 共 " << total_songs << " 首歌曲" << std::endl;
    for (const auto& [mood, songs] : mood_songs_) {
        std::cout << "  " << mood << "/: " << songs.size() << " 首" << std::endl;
    }
    return total_songs > 0;
}

std::vector<std::string> LocalMusicPlayer::scanMoodFolder(const std::string& folder_path) {
    std::vector<std::string> songs;
    DIR* dir = opendir(folder_path.c_str());
    if (!dir) return songs;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        if (hasAudioExtension(name)) {
            songs.push_back(folder_path + "/" + name);
        }
    }
    closedir(dir);
    return songs;
}

std::string LocalMusicPlayer::emotionToMoodFolder(Emotion e) {
    switch (e) {
        case Emotion::ANGRY:     return "angry";
        case Emotion::DISGUST:   return "disgust";
        case Emotion::FEAR:      return "fear";
        case Emotion::HAPPY:     return "happy";
        case Emotion::SAD:       return "sad";
        case Emotion::SURPRISED: return "surprised";
        case Emotion::NEUTRAL:   return "neutral";
        default:                 return "neutral";
    }
}

void LocalMusicPlayer::playForEmotion(Emotion emotion) {
    if (!initialized_) return;

    // 先停掉之前的播放
    stop();

    std::string mood = emotionToMoodFolder(emotion);
    auto it = mood_songs_.find(mood);
    if (it == mood_songs_.end() || it->second.empty()) {
        std::cout << "[LocalMusicPlayer] " << mood << " 文件夹无歌曲" << std::endl;
        return;
    }

    // 随机选一首
    int idx = std::rand() % it->second.size();
    std::string song = it->second[idx];

    std::cout << "[LocalMusicPlayer] 播放: " << song << std::endl;

    // fork + exec 播放
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程
        // 先尝试 mpv，再 ffplay
        execlp("mpv", "mpv", "--no-video", "--really-quiet", song.c_str(), (char*)nullptr);
        execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet",
               song.c_str(), (char*)nullptr);
        _exit(127);
    } else if (pid > 0) {
        child_pid_ = pid;
    } else {
        std::cerr << "[LocalMusicPlayer] fork 失败" << std::endl;
    }
}

void LocalMusicPlayer::stop() {
    if (child_pid_ > 0) {
        kill(child_pid_, SIGTERM);
        int status;
        waitpid(child_pid_, &status, 0);
        child_pid_ = -1;
    }
}

void LocalMusicPlayer::cleanup() {
    stop();
}

bool LocalMusicPlayer::isPlaying() {
    if (child_pid_ <= 0) return false;
    int status;
    pid_t ret = waitpid(child_pid_, &status, WNOHANG);
    if (ret == 0) return true;   // 仍在运行
    child_pid_ = -1;
    return false;
}

std::string LocalMusicPlayer::getLibraryInfo() const {
    int total = 0;
    std::string info;
    for (const auto& [mood, songs] : mood_songs_) {
        total += songs.size();
        info += mood + ":" + std::to_string(songs.size()) + " ";
    }
    return "共" + std::to_string(total) + "首 [" + info + "]";
}
