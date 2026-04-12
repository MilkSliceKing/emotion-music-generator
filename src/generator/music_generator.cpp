#include "music_generator.h"
#include <random>
#include <algorithm>

std::vector<int> MusicGenerator::getScaleIntervals(const std::string& scale) {
    if (scale == "major") return {0, 2, 4, 5, 7, 9, 11};
    if (scale == "minor") return {0, 2, 3, 5, 7, 8, 10};
    if (scale == "blues") return {0, 3, 5, 6, 7, 10};
    return {0, 2, 4, 5, 7, 9, 11}; // 默认大调
}

int MusicGenerator::keyToBaseNote(const std::string& key) {
    static const std::vector<std::string> keys = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        if (keys[i] == key) return 60 + i; // C4 = MIDI 60
    }
    return 60;
}

std::vector<int> MusicGenerator::generateMelody(int base_note,
                                                 const std::vector<int>& scale,
                                                 int num_notes) {
    std::mt19937 rng(std::random_device{}());

    // 构建两个八度的可用音符池
    std::vector<int> pool;
    for (int octave = 0; octave < 2; ++octave) {
        for (int interval : scale) {
            pool.push_back(base_note + octave * 12 + interval);
        }
    }

    std::vector<int> melody;
    melody.reserve(num_notes);

    // 第一个音固定为根音
    melody.push_back(base_note);

    // 后续音在音阶内随机选取，偏向相邻音
    std::uniform_int_distribution<int> step_dist(-2, 2);
    int current_idx = 0;

    for (int i = 1; i < num_notes; ++i) {
        int step = step_dist(rng);
        int new_idx = std::clamp(current_idx + step, 0, static_cast<int>(pool.size()) - 1);
        melody.push_back(pool[new_idx]);
        current_idx = new_idx;
    }

    return melody;
}

std::vector<Note> MusicGenerator::generate(const MusicParams& params, int num_notes) {
    std::vector<Note> notes;
    notes.reserve(num_notes);

    int base = keyToBaseNote(params.key);
    auto intervals = getScaleIntervals(params.scale);
    auto pitches = generateMelody(base, intervals, num_notes);

    double beat_duration = 60.0 / params.tempo; // 一拍的时长

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> vel_dist(
        std::max(0, params.velocity - 15),
        std::min(127, params.velocity + 15)
    );

    for (int pitch : pitches) {
        notes.push_back({pitch, beat_duration, vel_dist(rng)});
    }

    return notes;
}
