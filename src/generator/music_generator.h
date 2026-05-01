#ifndef MUSIC_GENERATOR_H
#define MUSIC_GENERATOR_H

#include <vector>
#include <string>
#include "mapper/emotion_mapper.h"

// 带时间戳的音符（用于旋律+伴奏叠加混音）
struct TimedNote {
    int pitch;         // MIDI 音高 (0-127, 60 = C4)
    double start_time; // 开始时间（秒，从曲首算起）
    double duration;   // 时值（秒）
    int velocity;      // 力度 (0-127)
};

class MusicGenerator {
public:
    MusicGenerator() = default;
    ~MusicGenerator() = default;

    // 生成完整乐曲（旋律+伴奏），返回带时间戳的音符列表
    std::vector<TimedNote> generateComposition(const MusicParams& params);

private:
    // 旋律用音阶半音间隔
    std::vector<int> getScaleIntervals(const std::string& scale);
    // 和弦构建用音阶（blues 用 minor 以获得完整三和弦）
    std::vector<int> getHarmonicIntervals(const std::string& scale);

    // 调性名 → MIDI 基准音（C4 = 60）
    int keyToBaseNote(const std::string& key);

    // 根据音阶度数构建三和弦（root, 3rd, 5th）
    std::vector<int> buildChord(int base, const std::vector<int>& scale,
                                int degree, int octave_offset = 0);

    // 生成一个 4 小节乐句的旋律
    std::vector<TimedNote> generateMelodyPhrase(
        int base,
        const std::vector<int>& harmonic_scale,
        const std::vector<int>& melody_scale,
        const std::vector<int>& phrase_chords,
        double start_time, double beat_dur, int velocity);

    // 生成一个 4 小节乐句的左手伴奏
    std::vector<TimedNote> generateAccompanimentPhrase(
        int base, const std::vector<int>& harmonic_scale,
        const std::vector<int>& phrase_chords,
        double start_time, double beat_dur, int velocity,
        const std::string& pattern);
};

#endif // MUSIC_GENERATOR_H
