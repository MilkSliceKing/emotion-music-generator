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

// ========== 旋律轮廓线 ==========

// 乐句弧线：16 个拍位的力度系数，先升后降
static double phraseArc(int bar, int beat) {
    double progress = (bar * 4 + beat) / 16.0;  // 0.0 ~ 0.9375
    double peak_pos = 0.625;  // 第 2.5 小节达到顶点
    double contour;
    if (progress < peak_pos) {
        contour = progress / peak_pos;
    } else {
        contour = (1.0 - progress) / (1.0 - peak_pos);
    }
    return 0.5 + 0.5 * contour;  // 范围 0.5 ~ 1.0
}

// 强弱拍模式
static double beatAccent(int beat) {
    static const double accents[] = {1.0, 0.7, 0.85, 0.65};
    return accents[beat % 4];
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
    bool recent_leap = false;  // 跳进后需要反向补偿

    // 节奏类型权重
    std::discrete_distribution<int> rhythm_dist({
        35,  // 四分音符
        25,  // 两个八分音符
        10,  // 附点四分+八分
        15,  // 二分音符
        15   // 休止符
    });

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

        for (int beat = 0; beat < 4; ++beat) {
            bool is_strong = (beat == 0 || beat == 2);
            bool is_last = (bar == 3 && beat >= 2);

            // 拍级力度 = 强弱拍 × 乐句弧线 × 随机微调
            double dynamic = beatAccent(beat) * phraseArc(bar, beat);
            dynamic *= 0.95 + std::uniform_real_distribution<double>(0.0, 0.1)(rng);
            double bar_vel = velocity * dynamic;

            // ---- 最后两拍：长音解决 ----
            if (is_last && beat == 2) {
                // 倒数第二拍：向根音靠拢
                int target;
                if (std::uniform_real_distribution<double>(0.0, 1.0)(rng) < 0.7) {
                    // 属音（五度）
                    target = base + 7;
                } else {
                    // 三度
                    target = base + 4;
                }
                notes.push_back({target, time, beat_dur, static_cast<int>(bar_vel * 0.8)});
                time += beat_dur;
                continue;
            }
            if (is_last && beat == 3) {
                // 最后一拍：根音长音收束
                notes.push_back({base, time, beat_dur * 2.0, static_cast<int>(bar_vel * 0.7)});
                time += beat_dur * 2.0;
                break;
            }

            // ---- 节奏选择 ----
            int rhythm_type = rhythm_dist(rng);

            if (rhythm_type == 4) {
                // 休止符：跳过这一拍
                time += beat_dur;
                continue;
            }

            // ---- 选音 ----
            int target_pitch;
            int prev_idx = current_idx;

            // 旋律轮廓目标音高
            double contour_progress = (bar * 4 + beat) / 16.0;
            double peak_pos = 0.625;
            double contour_val;
            if (contour_progress < peak_pos) {
                contour_val = contour_progress / peak_pos;
            } else {
                contour_val = (1.0 - contour_progress) / (1.0 - peak_pos);
            }
            int contour_pitch = base + static_cast<int>(contour_val * 12);

            if (is_strong) {
                std::uniform_real_distribution<double> prob(0.0, 1.0);
                double roll = prob(rng);

                if (recent_leap) {
                    // 跳进后：反向级进补偿
                    int dir = (current_idx > prev_idx) ? -1 : 1;
                    int new_idx = std::clamp(current_idx + dir, 0,
                                             static_cast<int>(pool.size()) - 1);
                    target_pitch = pool[new_idx];
                    recent_leap = false;
                } else if (roll < 0.20) {
                    // 20%：跳进（3-5 度到另一个和弦音）
                    int best_idx = current_idx;
                    int best_dist = 99;
                    for (int c : melody_chord) {
                        auto cit = std::find(pool.begin(), pool.end(), c);
                        if (cit != pool.end()) {
                            int ci = static_cast<int>(cit - pool.begin());
                            int dist = std::abs(ci - current_idx);
                            if (dist >= 3 && dist <= 7 && dist < best_dist) {
                                best_dist = dist;
                                best_idx = ci;
                            }
                        }
                    }
                    if (best_idx != current_idx) {
                        target_pitch = pool[best_idx];
                        recent_leap = true;
                    } else {
                        target_pitch = pool[current_idx];
                    }
                } else if (roll < 0.60) {
                    // 40%：最近的和弦音
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
                    // 40%：倾向轮廓目标音的和弦音
                    target_pitch = melody_chord[0];
                    int min_dist = std::abs(contour_pitch - melody_chord[0]);
                    for (int c : melody_chord) {
                        int dist = std::abs(contour_pitch - c);
                        if (dist < min_dist) {
                            min_dist = dist;
                            target_pitch = c;
                        }
                    }
                }
            } else {
                // 弱拍
                std::uniform_real_distribution<double> prob(0.0, 1.0);
                double roll = prob(rng);

                if (recent_leap) {
                    // 跳进后补偿
                    int dir = (current_idx > prev_idx) ? -1 : 1;
                    int new_idx = std::clamp(current_idx + dir, 0,
                                             static_cast<int>(pool.size()) - 1);
                    target_pitch = pool[new_idx];
                    recent_leap = false;
                } else if (roll < 0.15) {
                    // 15%：邻音（上或下一步，下一拍倾向回来）
                    int neighbor_dir = (std::uniform_int_distribution<int>(0, 1)(rng) == 0) ? 1 : -1;
                    int new_idx = std::clamp(current_idx + neighbor_dir, 0,
                                             static_cast<int>(pool.size()) - 1);
                    target_pitch = pool[new_idx];
                } else if (roll < 0.35) {
                    // 20%：倾向轮廓方向
                    int dir = (contour_pitch > pool[current_idx]) ? 1 : -1;
                    int new_idx = std::clamp(current_idx + dir, 0,
                                             static_cast<int>(pool.size()) - 1);
                    target_pitch = pool[new_idx];
                } else {
                    // 50%：级进（-1, 0, +1）
                    std::uniform_int_distribution<int> step(-1, 1);
                    int new_idx = std::clamp(current_idx + step(rng), 0,
                                             static_cast<int>(pool.size()) - 1);
                    target_pitch = pool[new_idx];
                }
            }

            // 更新当前位置
            auto it = std::find(pool.begin(), pool.end(), target_pitch);
            if (it != pool.end()) {
                current_idx = static_cast<int>(it - pool.begin());
            }

            // ---- 根据节奏类型生成音符 ----
            if (rhythm_type == 0) {
                // 四分音符
                notes.push_back({target_pitch, time, beat_dur,
                                 static_cast<int>(bar_vel)});
            } else if (rhythm_type == 1) {
                // 两个八分音符（第二个可能是经过音）
                int second_idx;
                int interval = current_idx - prev_idx;
                if (std::abs(interval) >= 2) {
                    // 大跳后插入经过音：中间位置
                    int mid_idx = (current_idx + prev_idx) / 2;
                    second_idx = std::clamp(mid_idx, 0,
                                            static_cast<int>(pool.size()) - 1);
                } else {
                    second_idx = std::clamp(
                        current_idx + std::uniform_int_distribution<int>(-1, 1)(rng),
                        0, static_cast<int>(pool.size()) - 1);
                }
                double eighth = beat_dur * 0.5;
                notes.push_back({target_pitch, time, eighth,
                                 static_cast<int>(bar_vel * 0.9)});
                notes.push_back({pool[second_idx], time + eighth, eighth,
                                 static_cast<int>(bar_vel * 0.85)});
                current_idx = second_idx;
            } else if (rhythm_type == 2) {
                // 附点四分 + 八分（摇摆感）
                double dotted = beat_dur * 0.75;
                double short_note = beat_dur * 0.25;
                notes.push_back({target_pitch, time, dotted,
                                 static_cast<int>(bar_vel)});
                // 短音：同方向级进
                int dir = (current_idx >= prev_idx) ? 1 : -1;
                int next_idx = std::clamp(current_idx + dir, 0,
                                          static_cast<int>(pool.size()) - 1);
                notes.push_back({pool[next_idx], time + dotted, short_note,
                                 static_cast<int>(bar_vel * 0.75)});
                current_idx = next_idx;
            } else if (rhythm_type == 3) {
                // 二分音符（跨两拍）
                notes.push_back({target_pitch, time, beat_dur * 2.0,
                                 static_cast<int>(bar_vel * 0.9)});
                time += beat_dur;  // 多跳一拍
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
