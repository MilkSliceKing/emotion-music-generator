#ifndef MUSIC_GENERATOR_H
#define MUSIC_GENERATOR_H

#include <vector>
#include <string>
#include "mapper/emotion_mapper.h"

// 音符结构体
struct Note {
    int pitch;       // MIDI 音高 (0-127, 60 = C4)
    double duration; // 时值 (秒)
    int velocity;    // 力度 (0-127)
};

class MusicGenerator {
public:
    MusicGenerator() = default;
    ~MusicGenerator() = default;

    // 根据音乐参数生成旋律
    std::vector<Note> generate(const MusicParams& params, int num_notes = 8);

private:
    // 获取音阶的半音间隔
    std::vector<int> getScaleIntervals(const std::string& scale);

    // 调性名 → MIDI 基准音
    int keyToBaseNote(const std::string& key);

    // 生成旋律音高序列
    std::vector<int> generateMelody(int base_note,
                                    const std::vector<int>& scale,
                                    int num_notes);
};

#endif // MUSIC_GENERATOR_H
