#include "music_generator.h"
#include <random>
#include <algorithm>
#include <cmath>

// ========== 音阶 ==========

std::vector<int> MusicGenerator::getScaleIntervals(const std::string& scale) {
    if (scale == "major") return {0, 2, 4, 5, 7, 9, 11};
    if (scale == "minor") return {0, 2, 3, 5, 7, 8, 10};
    if (scale == "blues") return {0, 3, 5, 6, 7, 10};       // 旋律用（含蓝调音）
    return {0, 2, 4, 5, 7, 9, 11};
}

std::vector<int> MusicGenerator::getHarmonicIntervals(const std::string& scale) {
    // 和弦构建统一用 7 音音阶，blues 用 natural minor
    if (scale == "blues") return {0, 2, 3, 5, 7, 8, 10};
    return getScaleIntervals(scale);
}

int MusicGenerator::keyToBaseNote(const std::string& key) {
    static const std::vector<std::string> keys = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        if (keys[i] == key) return 60 + i;   // C4 = MIDI 60
    }
    return 60;
}

// ========== 和弦构建 ==========

std::vector<int> MusicGenerator::buildChord(int base, const std::vector<int>& scale,
                                             int degree, int octave_offset) {
    int n = static_cast<int>(scale.size());
    std::vector<int> chord;

    // Root
    int r_oct = degree / n;
    chord.push_back(base + (octave_offset + r_oct) * 12 + scale[degree % n]);

    // Third
    int t_deg = degree + 2;
    int t_oct = t_deg / n;
    chord.push_back(base + (octave_offset + t_oct) * 12 + scale[t_deg % n]);

    // Fifth
    int f_deg = degree + 4;
    int f_oct = f_deg / n;
    chord.push_back(base + (octave_offset + f_oct) * 12 + scale[f_deg % n]);

    return chord;
}

// ========== 旋律生成（一个 4 小节乐句）==========

std::vector<TimedNote> MusicGenerator::generateMelodyPhrase(
    int base,
    const std::vector<int>& harmonic_scale,
    const std::vector<int>& melody_scale,
    const std::vector<int>& phrase_chords,
    double start_time, double beat_dur, int velocity)
{
    std::mt19937 rng(std::random_device{}());
    std::vector<TimedNote> notes;

    // 构建旋律音池（两个半八度，覆盖旋律常用音区）
    std::vector<int> pool;
    for (int oct = 0; oct < 3; ++oct) {
        for (int interval : melody_scale) {
            pool.push_back(base + oct * 12 + interval);
        }
    }

    // 初始位置：根音附近
    auto root_it = std::find(pool.begin(), pool.end(), base);
    int current_idx = static_cast<int>(root_it - pool.begin());
    if (current_idx >= static_cast<int>(pool.size())) current_idx = 0;

    double time = start_time;

    // 乐句力度曲线：起→承→转→合
    double dynamics[] = {0.65, 0.80, 1.0, 0.55};

    for (int bar = 0; bar < 4; ++bar) {
        int chord_deg = phrase_chords[bar];
        auto chord = buildChord(base, harmonic_scale, chord_deg);

        // 将和弦音提升到旋律音区
        std::vector<int> melody_chord;
        for (int c : chord) {
            while (c < base) c += 12;
            while (c > base + 24) c -= 12;
            melody_chord.push_back(c);
        }

        double bar_vel = velocity * dynamics[bar];

        for (int beat = 0; beat < 4; ++beat) {
            bool is_strong = (beat == 0 || beat == 2);
            bool is_last = (bar == 3 && beat >= 2);

            // ---- 最后两拍：长音解决到根音 ----
            if (is_last && beat == 2) {
                // 倒数第二拍：属音（五度）或三度
                int penult = (current_idx + 2 < static_cast<int>(pool.size()))
                             ? pool[current_idx + 2] : base + 7;
                notes.push_back({penult, time, beat_dur, static_cast<int>(bar_vel * 0.8)});
                time += beat_dur;
                continue;
            }
            if (is_last && beat == 3) {
                // 最后一拍：根音长音收束
                notes.push_back({base, time, beat_dur * 2.0, static_cast<int>(bar_vel * 0.7)});
                time += beat_dur * 2.0;
                break;
            }

            // ---- 正常音符 ----
            int target_pitch;

            if (is_strong) {
                std::uniform_real_distribution<double> prob(0.0, 1.0);
                if (prob(rng) < 0.7) {
                    // 70%：选最近的和弦音
                    target_pitch = melody_chord[0];
                    int min_dist = std::abs(pool[current_idx] - melody_chord[0]);
                    for (int c : melody_chord) {
                        int dist = std::abs(pool[current_idx] - c);
                        if (dist < min_dist) {
                            min_dist = dist;
                            target_pitch = c;
                        }
                    }
                } else {
                    // 30%：级进
                    std::uniform_int_distribution<int> step(-2, 2);
                    int new_idx = std::clamp(current_idx + step(rng), 0,
                                             static_cast<int>(pool.size()) - 1);
                    target_pitch = pool[new_idx];
                }
            } else {
                // 弱拍：级进（-1, 0, +1）
                std::uniform_int_distribution<int> step(-1, 1);
                int new_idx = std::clamp(current_idx + step(rng), 0,
                                         static_cast<int>(pool.size()) - 1);
                target_pitch = pool[new_idx];
            }

            // 更新当前位置
            auto it = std::find(pool.begin(), pool.end(), target_pitch);
            if (it != pool.end()) {
                current_idx = static_cast<int>(it - pool.begin());
            }

            // 节奏：强拍可能拆成两个八分音符
            std::uniform_real_distribution<double> rhythm_prob(0.0, 1.0);
            if (is_strong && rhythm_prob(rng) < 0.4) {
                // 两个八分音符
                int second_idx = std::clamp(
                    current_idx + std::uniform_int_distribution<int>(-1, 1)(rng),
                    0, static_cast<int>(pool.size()) - 1);
                double eighth = beat_dur * 0.5;
                notes.push_back({target_pitch, time, eighth,
                                 static_cast<int>(bar_vel * 0.9)});
                notes.push_back({pool[second_idx], time + eighth, eighth,
                                 static_cast<int>(bar_vel * 0.85)});
                current_idx = second_idx;
            } else {
                // 四分音符
                notes.push_back({target_pitch, time, beat_dur,
                                 static_cast<int>(bar_vel)});
            }
            time += beat_dur;
        }
    }

    return notes;
}

// ========== 伴奏生成（一个 4 小节乐句）==========

std::vector<TimedNote> MusicGenerator::generateAccompanimentPhrase(
    int base, const std::vector<int>& harmonic_scale,
    const std::vector<int>& phrase_chords,
    double start_time, double beat_dur, int velocity,
    const std::string& pattern)
{
    std::vector<TimedNote> notes;
    int acc_vel = static_cast<int>(velocity * 0.50);   // 伴奏力度低于旋律
    int acc_base = base - 12;                           // 伴奏低一个八度

    for (int bar = 0; bar < 4; ++bar) {
        int chord_deg = phrase_chords[bar];
        auto chord = buildChord(acc_base, harmonic_scale, chord_deg);
        double bar_time = start_time + bar * 4.0 * beat_dur;

        if (pattern == "arpeggiated") {
            // 分解和弦：每拍一个音，root → 3rd → 5th → 3rd 循环
            int idx_map[] = {0, 1, 2, 1};
            for (int beat = 0; beat < 4; ++beat) {
                int ci = idx_map[beat];
                if (ci < static_cast<int>(chord.size())) {
                    notes.push_back({chord[ci], bar_time + beat * beat_dur,
                                     beat_dur * 0.85, acc_vel});
                }
            }

        } else if (pattern == "block") {
            // 柱式和弦：第 1、3 拍同时弹三个音
            for (int beat : {0, 2}) {
                for (int c : chord) {
                    notes.push_back({c, bar_time + beat * beat_dur,
                                     beat_dur * 1.8, acc_vel});
                }
            }

        } else if (pattern == "alberti") {
            // 阿尔贝蒂低音：八分音符 root-5th-3rd-5th 重复
            int idx_map[] = {0, 2, 1, 2};
            double eighth = beat_dur * 0.5;
            for (int beat = 0; beat < 4; ++beat) {
                for (int sub = 0; sub < 2; ++sub) {
                    int ci = idx_map[(beat * 2 + sub) % 4];
                    if (ci < static_cast<int>(chord.size())) {
                        notes.push_back({chord[ci],
                                         bar_time + beat * beat_dur + sub * eighth,
                                         eighth * 0.8, acc_vel});
                    }
                }
            }
        }
    }

    return notes;
}

// ========== 主生成函数 ==========

std::vector<TimedNote> MusicGenerator::generateComposition(const MusicParams& params) {
    std::vector<TimedNote> composition;

    int base = keyToBaseNote(params.key);
    auto harmonic_scale = getHarmonicIntervals(params.scale);
    auto melody_scale = getScaleIntervals(params.scale);
    double beat_dur = 60.0 / params.tempo;

    int num_phrases = std::max(1, params.num_phrases);
    double time = 0.0;

    for (int phrase = 0; phrase < num_phrases; ++phrase) {
        // 每个乐句用完整和弦进行（4 和弦）
        std::vector<int> phrase_chords;
        for (int i = 0; i < 4; ++i) {
            phrase_chords.push_back(params.progression[i % params.progression.size()]);
        }

        // 最后一个乐句末尾 V→I 经典解决
        if (phrase == num_phrases - 1) {
            int n = static_cast<int>(harmonic_scale.size());
            phrase_chords[2] = 4 % n;   // V 级
            phrase_chords[3] = 0;       // I 级（主和弦）
        }

        // 生成旋律
        auto melody = generateMelodyPhrase(base, harmonic_scale, melody_scale,
                                           phrase_chords, time, beat_dur, params.velocity);
        composition.insert(composition.end(), melody.begin(), melody.end());

        // 生成伴奏
        auto acc = generateAccompanimentPhrase(base, harmonic_scale, phrase_chords,
                                               time, beat_dur, params.velocity,
                                               params.accompaniment);
        composition.insert(composition.end(), acc.begin(), acc.end());

        // 推进时间：4 小节 × 4 拍
        time += 4.0 * 4.0 * beat_dur;

        // 乐句间呼吸休止
        if (phrase < num_phrases - 1) {
            time += 0.3;
        }
    }

    return composition;
}
