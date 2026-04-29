#include "music_generator.h"
#include <random>
#include <algorithm>
#include <map>
#include <sstream>
#include <cmath>

// ========== 音阶/调式工具 ==========

std::vector<int> MusicGenerator::getScaleIntervals(const std::string& scale) {
    if (scale == "major") return {0, 2, 4, 5, 7, 9, 11};
    if (scale == "minor") return {0, 2, 3, 5, 7, 8, 10};
    if (scale == "blues") return {0, 3, 5, 6, 7, 10};
    return {0, 2, 4, 5, 7, 9, 11};
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

// ========== 和弦进行解析 ==========

// 音阶度数信息：半音偏移 + 和弦性质
struct DegreeInfo {
    int semitone_offset;       // 相对于调式根音的半音数
    std::string chord_quality; // "maj", "min", "dim"
};

static std::map<int, DegreeInfo> buildMajorDegrees() {
    // 大调: I(maj), ii(min), iii(min), IV(maj), V(maj), vi(min), vii°(dim)
    return {
        {1, {0,  "maj"}},
        {2, {2,  "min"}},
        {3, {4,  "min"}},
        {4, {5,  "maj"}},
        {5, {7,  "maj"}},
        {6, {9,  "min"}},
        {7, {11, "dim"}}
    };
}

static std::map<int, DegreeInfo> buildMinorDegrees() {
    // 自然小调: i(min), ii°(dim), III(maj), iv(min), V(maj), VI(maj), VII(maj)
    return {
        {1, {0,  "min"}},
        {2, {2,  "dim"}},
        {3, {3,  "maj"}},
        {4, {5,  "min"}},
        {5, {7,  "maj"}},
        {6, {8,  "maj"}},
        {7, {10, "maj"}}
    };
}

// 解析单个罗马数字 → 度数 (1-7)
// 大写 = 大调度数，小写 = 小调度数（我们忽略大小写差异，用调式决定性质）
static int romanToDegree(const std::string& roman) {
    static const std::map<std::string, int> roman_map = {
        {"I", 1}, {"II", 2}, {"III", 3}, {"IV", 4}, {"V", 5}, {"VI", 6}, {"VII", 7},
        {"i", 1}, {"ii", 2}, {"iii", 3}, {"iv", 4}, {"v", 5}, {"vi", 6}, {"vii", 7}
    };
    auto it = roman_map.find(roman);
    return (it != roman_map.end()) ? it->second : 1;
}

// 和弦性质 → 半音偏移列表
static std::vector<int> qualityToIntervals(const std::string& quality) {
    if (quality == "maj")  return {0, 4, 7};
    if (quality == "min")  return {0, 3, 7};
    if (quality == "dim")  return {0, 3, 6};
    return {0, 4, 7}; // 默认大三和弦
}

std::vector<ChordDef> MusicGenerator::parseProgression(const std::string& progression,
                                                        int base_note,
                                                        const std::string& scale) {
    // 选择调式度数表
    auto degrees = (scale == "minor" || scale == "blues")
                   ? buildMinorDegrees()
                   : buildMajorDegrees();

    std::vector<ChordDef> chords;
    std::istringstream iss(progression);
    std::string token;

    while (std::getline(iss, token, '-')) {
        int deg = romanToDegree(token);
        auto it = degrees.find(deg);
        if (it == degrees.end()) continue;

        int root = base_note + it->second.semitone_offset;
        // blues 音阶统一用小三和弦
        std::string quality = (scale == "blues") ? "min" : it->second.chord_quality;

        chords.push_back({token, qualityToIntervals(quality), root});
    }

    return chords;
}

// ========== 旋律生成（基于和弦音 + 经过音）==========

std::vector<Note> MusicGenerator::generateMelodyForChord(
    const ChordDef& chord,
    const ChordDef* next_chord,
    double measure_start,
    double beat_duration,
    int base_velocity,
    double intensity,
    const std::vector<int>& scale_intervals,
    int base_note)
{
    std::mt19937 rng(std::random_device{}());
    std::vector<Note> notes;

    // 构建两个八度的音阶音池
    std::vector<int> scale_pool;
    for (int oct = -1; oct <= 2; ++oct) {
        for (int interval : scale_intervals) {
            scale_pool.push_back(base_note + oct * 12 + interval);
        }
    }
    // 排序去重
    std::sort(scale_pool.begin(), scale_pool.end());
    scale_pool.erase(std::unique(scale_pool.begin(), scale_pool.end()), scale_pool.end());

    // 获取和弦音（在旋律音域范围：MIDI 60-84，即 C4-C6）
    std::vector<int> chord_tones;
    for (int oct = 0; oct <= 1; ++oct) {
        for (int interval : chord.intervals) {
            int pitch = chord.root_pitch + oct * 12 + interval;
            if (pitch >= 58 && pitch <= 86) {
                chord_tones.push_back(pitch);
            }
        }
    }
    std::sort(chord_tones.begin(), chord_tones.end());

    // 如果有下一个和弦，获取其导音
    std::vector<int> next_chord_tones;
    if (next_chord) {
        for (int oct = 0; oct <= 1; ++oct) {
            for (int interval : next_chord->intervals) {
                int pitch = next_chord->root_pitch + oct * 12 + interval;
                if (pitch >= 58 && pitch <= 86) {
                    next_chord_tones.push_back(pitch);
                }
            }
        }
    }

    // 力度变化范围
    int vel_min = std::max(0, static_cast<int>(base_velocity * intensity * 0.85));
    int vel_max = std::min(127, static_cast<int>(base_velocity * intensity * 1.15));
    std::uniform_int_distribution<int> vel_dist(vel_min, vel_max);

    // 生成 4 拍的旋律
    static const double intensity_curve[] = {0.6, 0.8, 1.0, 0.7};
    int last_pitch = chord_tones[0]; // 从和弦音开始

    for (int beat = 0; beat < 4; ++beat) {
        double beat_start = measure_start + beat * beat_duration;
        double beat_intensity = intensity_curve[beat];
        int vel = std::max(30, std::min(127,
            static_cast<int>(vel_dist(rng) * beat_intensity)));

        int target_pitch;

        if (beat == 0 || beat == 2) {
            // 强拍：70% 和弦音，30% 音阶音
            std::uniform_int_distribution<int> coin(1, 10);
            if (coin(rng) <= 7 && !chord_tones.empty()) {
                // 选和弦音，倾向于靠近上一个音
                std::vector<int> candidates;
                for (int ct : chord_tones) {
                    if (std::abs(ct - last_pitch) <= 7) {
                        candidates.push_back(ct);
                    }
                }
                if (candidates.empty()) candidates = chord_tones;

                // 从候选音中选距离最近的
                std::sort(candidates.begin(), candidates.end(),
                    [last_pitch](int a, int b) {
                        return std::abs(a - last_pitch) < std::abs(b - last_pitch);
                    });
                // 前3个中随机选
                int pick_count = std::min(3, static_cast<int>(candidates.size()));
                std::uniform_int_distribution<int> pick_dist(0, pick_count - 1);
                target_pitch = candidates[pick_dist(rng)];
            } else {
                // 音阶音，步进选择
                auto it = std::lower_bound(scale_pool.begin(), scale_pool.end(), last_pitch);
                int idx = static_cast<int>(it - scale_pool.begin());
                std::uniform_int_distribution<int> step_dist(-2, 2);
                int new_idx = std::clamp(idx + step_dist(rng), 0,
                    static_cast<int>(scale_pool.size()) - 1);
                target_pitch = scale_pool[new_idx];
            }
        } else if (beat == 3) {
            // 弱拍(第4拍)：倾向导向下一个和弦音
            if (!next_chord_tones.empty()) {
                // 找最近的目标和弦音
                std::vector<int> leading_tones;
                for (int nct : next_chord_tones) {
                    int dist = std::abs(nct - last_pitch);
                    if (dist <= 5) leading_tones.push_back(nct);
                }
                if (leading_tones.empty()) leading_tones = next_chord_tones;

                std::sort(leading_tones.begin(), leading_tones.end(),
                    [last_pitch](int a, int b) {
                        return std::abs(a - last_pitch) < std::abs(b - last_pitch);
                    });

                // 导音：目标音下方的二度或直接解决
                int target = leading_tones[0];
                // 50% 直接跳到目标，50% 用经过音
                std::uniform_int_distribution<int> coin(0, 1);
                if (coin(rng) || std::abs(target - last_pitch) <= 2) {
                    target_pitch = target;
                } else {
                    // 经过音：目标音下方一个音阶音
                    int step = (target > last_pitch) ? -1 : 1;
                    auto it = std::lower_bound(scale_pool.begin(), scale_pool.end(), target);
                    int idx = static_cast<int>(it - scale_pool.begin()) + step;
                    idx = std::clamp(idx, 0, static_cast<int>(scale_pool.size()) - 1);
                    target_pitch = scale_pool[idx];
                }
            } else {
                // 没有下一个和弦，回到当前和弦音
                std::sort(chord_tones.begin(), chord_tones.end(),
                    [last_pitch](int a, int b) {
                        return std::abs(a - last_pitch) < std::abs(b - last_pitch);
                    });
                target_pitch = chord_tones[0];
            }
        } else {
            // 弱拍(第2拍)：经过音或和弦音
            std::uniform_int_distribution<int> coin(1, 10);
            if (coin(rng) <= 4 && !chord_tones.empty()) {
                // 和弦音
                std::sort(chord_tones.begin(), chord_tones.end(),
                    [last_pitch](int a, int b) {
                        return std::abs(a - last_pitch) < std::abs(b - last_pitch);
                    });
                int pick_count = std::min(3, static_cast<int>(chord_tones.size()));
                std::uniform_int_distribution<int> pick_dist(0, pick_count - 1);
                target_pitch = chord_tones[pick_dist(rng)];
            } else {
                // 步进经过音
                auto it = std::lower_bound(scale_pool.begin(), scale_pool.end(), last_pitch);
                int idx = static_cast<int>(it - scale_pool.begin());
                std::uniform_int_distribution<int> step_dist(-2, 2);
                int new_idx = std::clamp(idx + step_dist(rng), 0,
                    static_cast<int>(scale_pool.size()) - 1);
                target_pitch = scale_pool[new_idx];
            }
        }

        last_pitch = target_pitch;

        // 时值：根据情绪强度选择
        double dur;
        std::uniform_int_distribution<int> dur_choice(0, 9);
        int dc = dur_choice(rng);
        if (dc < 4) {
            dur = beat_duration;       // 四分音符
        } else if (dc < 7) {
            dur = beat_duration * 0.5; // 八分音符
        } else {
            dur = beat_duration * 2.0; // 二分音符
        }

        // 快板情绪倾向短时值
        if (base_velocity > 100 && dc < 5) {
            dur = beat_duration * 0.5;
        }
        // 慢板情绪倾向长时值
        if (base_velocity < 70 && dc < 5) {
            dur = beat_duration;
        }

        notes.push_back({target_pitch, beat_start, dur, vel, false});
    }

    return notes;
}

// ========== 伴奏生成 ==========

std::vector<Note> MusicGenerator::generateAccompaniment(
    const std::vector<ChordDef>& chords,
    const MusicParams& params,
    double beat_duration)
{
    std::vector<Note> accompaniment;
    int measures_per_chord = 1;
    double measure_duration = beat_duration * 4;

    for (size_t ci = 0; ci < chords.size(); ++ci) {
        const auto& chord = chords[ci];
        double measure_start = ci * measures_per_chord * measure_duration;

        // 伴奏音域：低一个八度
        int bass_root = chord.root_pitch - 12;

        if (params.accompaniment == "arpeggio") {
            // 分解和弦：每拍一个音，根音→三度→五度→根音
            for (int beat = 0; beat < 4; ++beat) {
                int interval_idx = beat % chord.intervals.size();
                int pitch = bass_root + chord.intervals[interval_idx];
                double start = measure_start + beat * beat_duration;
                int vel = static_cast<int>(params.velocity * 0.5);
                accompaniment.push_back({pitch, start, beat_duration * 0.9, vel, false});
            }
        } else if (params.accompaniment == "block") {
            // 柱式和弦：每小节第1拍同时弹三个音
            for (int interval : chord.intervals) {
                int pitch = bass_root + interval;
                int vel = static_cast<int>(params.velocity * 0.45);
                accompaniment.push_back({pitch, measure_start, beat_duration * 2.0, vel, false});
            }
        } else if (params.accompaniment == "alberti") {
            // 阿尔贝蒂低音：根音→五度→三度→五度，每半拍一个
            // intervals: [0, 3or4, 7]，即 root, 3rd, 5th
            int root_i = 0;
            int third_i = (chord.intervals.size() > 1) ? 1 : 0;
            int fifth_i = (chord.intervals.size() > 2) ? 2 : 0;

            int pattern[] = {root_i, fifth_i, third_i, fifth_i,
                             root_i, fifth_i, third_i, fifth_i};

            for (int sub = 0; sub < 8; ++sub) {
                int pitch = bass_root + chord.intervals[pattern[sub]];
                double start = measure_start + sub * beat_duration * 0.5;
                int vel = static_cast<int>(params.velocity * 0.45);
                accompaniment.push_back(
                    {pitch, start, beat_duration * 0.45, vel, false});
            }
        }
        // "none" 不生成伴奏
    }

    return accompaniment;
}

// ========== 主生成接口 ==========

Composition MusicGenerator::generateComposition(const MusicParams& params, int num_phrases) {
    Composition comp;
    comp.tempo = params.tempo;
    comp.mood = params.mood;

    int base = keyToBaseNote(params.key);
    auto scale_intervals = getScaleIntervals(params.scale);
    auto chords = parseProgression(params.progression, base, params.scale);

    if (chords.empty()) {
        // 回退：简单的 I 和弦
        chords.push_back({"I", {0, 4, 7}, base});
    }

    double beat_duration = 60.0 / params.tempo;
    double measure_duration = beat_duration * 4;

    // 乐句力度曲线：起→承→转→合
    static const double phrase_intensity[] = {0.6, 0.8, 1.0, 0.7};

    std::mt19937 rng(std::random_device{}());

    for (int phrase = 0; phrase < num_phrases; ++phrase) {
        double phrase_start = phrase * chords.size() * measure_duration;

        for (size_t ci = 0; ci < chords.size(); ++ci) {
            double measure_start = phrase_start + ci * measure_duration;
            double intensity = phrase_intensity[ci % 4];

            // 是否是整个乐曲的最后一小节
            bool is_last_measure = (phrase == num_phrases - 1) &&
                                   (ci == chords.size() - 1);

            // 下一个和弦（用于导音）
            const ChordDef* next_chord = nullptr;
            if (ci + 1 < chords.size()) {
                next_chord = &chords[ci + 1];
            } else if (phrase + 1 < num_phrases) {
                // 下一个乐句的第一个和弦
                next_chord = &chords[0];
            }

            auto melody_notes = generateMelodyForChord(
                chords[ci], next_chord, measure_start, beat_duration,
                params.velocity, intensity, scale_intervals, base
            );

            for (auto& note : melody_notes) {
                comp.melody.push_back(note);
            }

            // 乐句结尾：加半拍休止
            if (ci == chords.size() - 1) {
                double rest_start = measure_start + measure_duration;
                comp.melody.push_back({0, rest_start, beat_duration * 0.5, 0, true});
            }
        }
    }

    // 末尾解决：确保最后一个音是根音
    if (!comp.melody.empty()) {
        // 替换最后一个非休止音符为根音
        for (int i = static_cast<int>(comp.melody.size()) - 1; i >= 0; --i) {
            if (!comp.melody[i].is_rest) {
                // 倒数第二个音用五度（V-I 解决）
                if (i > 0 && !comp.melody[i-1].is_rest) {
                    int fifth_pitch = chords.back().root_pitch + 7;
                    // 确保在合理范围
                    while (fifth_pitch > 84) fifth_pitch -= 12;
                    while (fifth_pitch < 58) fifth_pitch += 12;
                    comp.melody[i-1].pitch = fifth_pitch;
                }
                // 最后一个音解决到根音
                int root_pitch = chords[0].root_pitch;
                while (root_pitch > 84) root_pitch -= 12;
                while (root_pitch < 58) root_pitch += 12;
                comp.melody[i].pitch = root_pitch;
                comp.melody[i].duration = beat_duration * 2.0; // 长音收尾
                break;
            }
        }
    }

    // 生成伴奏（只生成一个乐句的长度，重复 num_phrases 次）
    auto one_phrase_acc = generateAccompaniment(chords, params, beat_duration);
    double phrase_duration = chords.size() * measure_duration;

    for (int phrase = 0; phrase < num_phrases; ++phrase) {
        double offset = phrase * phrase_duration;
        for (const auto& note : one_phrase_acc) {
            Note shifted = note;
            shifted.start_time += offset;
            comp.accompaniment.push_back(shifted);
        }
    }

    // 计算总时长
    double melody_end = 0, acc_end = 0;
    for (const auto& n : comp.melody) {
        if (!n.is_rest) {
            melody_end = std::max(melody_end, n.start_time + n.duration);
        }
    }
    for (const auto& n : comp.accompaniment) {
        acc_end = std::max(acc_end, n.start_time + n.duration);
    }
    comp.total_duration = std::max(melody_end, acc_end) + 0.1; // 加一点余量

    return comp;
}

// ========== 旧接口兼容 ==========

std::vector<Note> MusicGenerator::generate(const MusicParams& params, int num_notes) {
    auto comp = generateComposition(params, 1);
    return comp.melody;
}
