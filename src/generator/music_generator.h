#ifndef MUSIC_GENERATOR_H
#define MUSIC_GENERATOR_H

#include <vector>
#include <string>
#include "mapper/emotion_mapper.h"

// 音符结构体
struct Note {
    int pitch;          // MIDI 音高 (0-127, 60 = C4)
    double start_time;  // 绝对起始时间 (秒)
    double duration;    // 时值 (秒)
    int velocity;       // 力度 (0-127)
    bool is_rest;       // 是否为休止符
};

// 和弦定义
struct ChordDef {
    std::string name;           // 名称，如 "I", "V", "vi"
    std::vector<int> intervals; // 和弦音的半音偏移，如 {0,4,7} (大三和弦)
    int root_pitch;             // 根音的 MIDI 音高
};

// 完整的乐曲（旋律 + 伴奏）
struct Composition {
    std::vector<Note> melody;       // 右手旋律
    std::vector<Note> accompaniment;// 左手伴奏
    int tempo;                      // BPM
    std::string mood;               // 音色标签
    double total_duration;          // 总时长 (秒)
};

class MusicGenerator {
public:
    MusicGenerator() = default;
    ~MusicGenerator() = default;

    // 生成完整乐曲（旋律 + 伴奏）
    Composition generateComposition(const MusicParams& params, int num_phrases = 2);

    // 旧接口兼容（仅生成旋律）
    std::vector<Note> generate(const MusicParams& params, int num_notes = 8);

private:
    // 获取音阶的半音间隔
    std::vector<int> getScaleIntervals(const std::string& scale);

    // 调性名 → MIDI 基准音
    int keyToBaseNote(const std::string& key);

    // 解析和弦进行字符串 → 和弦列表
    std::vector<ChordDef> parseProgression(const std::string& progression,
                                            int base_note,
                                            const std::string& scale);

    // 为一个和弦生成一小时的旋律音符
    std::vector<Note> generateMelodyForChord(const ChordDef& chord,
                                              const ChordDef* next_chord,
                                              double measure_start,
                                              double beat_duration,
                                              int base_velocity,
                                              double intensity,
                                              const std::vector<int>& scale_intervals,
                                              int base_note);

    // 生成左手伴奏
    std::vector<Note> generateAccompaniment(const std::vector<ChordDef>& chords,
                                             const MusicParams& params,
                                             double beat_duration);
};

#endif // MUSIC_GENERATOR_H
