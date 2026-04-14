#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <vector>
#include <string>
#include "generator/music_generator.h"

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    bool init();
    void play(const std::vector<Note>& notes);
    void stop();
    void cleanup();

private:
    bool initialized_;
    bool playing_;

    // 生成正弦波音频数据
    std::vector<float> generateSineWave(int pitch, double duration,
                                         int sample_rate = 44100);

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
