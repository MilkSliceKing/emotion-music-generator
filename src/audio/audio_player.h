#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <vector>
#include <string>
#include "generator/music_generator.h"

// 音色类型
enum class Timbre {
    BRIGHT_PIANO,   // 明亮钢琴 (Happy, Surprised)
    MELLOW_PIANO,   // 柔和钢琴 (Neutral)
    SOFT_STRINGS,   // 柔和弦乐 (Sad)
    DARK_PAD,       // 暗色铺底 (Disgust)
    PLUCKED,        // 拨弦/吉他 (Fear)
    HARSH           // 粗犷/失真 (Angry)
};

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    bool init();
    void play(const std::vector<Note>& notes, const std::string& mood = "calm");
    void stop();
    void cleanup();
    bool isPlaying();

    // mood 字符串转音色类型
    static Timbre moodToTimbre(const std::string& mood);

private:
    bool initialized_;
    bool playing_;
    pid_t child_pid_ = -1;

    // 根据音色生成波形
    std::vector<float> generateNote(int pitch, double duration,
                                     Timbre timbre, int velocity,
                                     int sample_rate = 44100);

    // 各音色的合成方法
    std::vector<float> synthPiano(int pitch, double duration, int velocity, int sample_rate);
    std::vector<float> synthStrings(int pitch, double duration, int velocity, int sample_rate);
    std::vector<float> synthPad(int pitch, double duration, int velocity, int sample_rate);
    std::vector<float> synthPlucked(int pitch, double duration, int velocity, int sample_rate);
    std::vector<float> synthHarsh(int pitch, double duration, int velocity, int sample_rate);

    // MIDI 音高 → 频率 (Hz)
    double midiToFrequency(int pitch);

    // 将音频数据写入 WAV 文件
    bool writeWavFile(const std::string& filename,
                      const std::vector<float>& samples,
                      int sample_rate = 44100);

    // 查找可用的音频播放命令
    std::string findPlayerCommand();
};

#endif // AUDIO_PLAYER_H
