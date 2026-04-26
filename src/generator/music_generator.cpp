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
    int root_idx = 0;  // 根音在 pool 中的位置

    for (int i = 1; i < num_notes; ++i) {
        int step = step_dist(rng);

        // 越接近结尾，越倾向回到根音附近
        double progress = static_cast<double>(i) / num_notes;
        if (progress > 0.6) {
            // 计算当前到根音的距离
            int dist_to_root = root_idx - current_idx;
            // 加入向根音的引力，越接近结尾引力越大
            double gravity = (progress - 0.6) / 0.4;  // 0.0 → 1.0
            step += static_cast<int>(dist_to_root * gravity * 0.5);
        }

        int new_idx = std::clamp(current_idx + step, 0, static_cast<int>(pool.size()) - 1);
        melody.push_back(pool[new_idx]);
        current_idx = new_idx;
    }

    // 最后一个音回归根音（解决感）
    melody.push_back(base_note);

    return melody;
}

std::vector<Note> MusicGenerator::generate(const MusicParams& params, int num_notes) {
    std::vector<Note> notes;
    notes.reserve(num_notes);

    int base = keyToBaseNote(params.key);
    auto intervals = getScaleIntervals(params.scale);
    auto pitches = generateMelody(base, intervals, num_notes);

    double beat_duration = 60.0 / params.tempo; // 一拍的时长

    // 节奏模式：不同时值权重（全音/半音/四分音符）
    // 权重越高越常出现
    static const double duration_patterns[] = {
        2.0, 2.0,   // 二分音符 (两拍)
        1.0, 1.0, 1.0, 1.0, // 四分音符 (一拍) — 最常见
        0.5, 0.5, 0.5       // 八分音符 (半拍)
    };
    static const int num_patterns = sizeof(duration_patterns) / sizeof(duration_patterns[0]);

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> vel_dist(
        std::max(0, params.velocity - 15),
        std::min(127, params.velocity + 15)
    );
    std::uniform_int_distribution<int> rhythm_dist(0, num_patterns - 1);

    int total = static_cast<int>(pitches.size());
    for (int i = 0; i < total; ++i) {
        double dur;
        // 最后一个音固定为长音（收尾）
        if (i == total - 1) {
            dur = beat_duration * 2.0;
        } else {
            dur = beat_duration * duration_patterns[rhythm_dist(rng)];
        }
        notes.push_back({pitches[i], dur, vel_dist(rng)});
    }

    return notes;
}
