#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <vector>
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
    void* stream_;  // PaStream* (使用 void* 避免头文件依赖)

    // 生成正弦波音频数据
    std::vector<float> generateSineWave(int pitch, double duration,
                                         int sample_rate = 44100);

    // MIDI 音高 → 频率 (Hz)
    double midiToFrequency(int pitch);
};

#endif // AUDIO_PLAYER_H
