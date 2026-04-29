#include "emotion_mapper.h"

// 情绪 → 音乐参数的映射表
// FER+ 枚举: NEUTRAL=0, HAPPY=1, SURPRISED=2, SAD=3, ANGRY=4, DISGUST=5, FEAR=6

const MusicParams EmotionMapper::HAPPY_PARAMS     = {140, "C", "major", 100, "bright",     "I-V-vi-IV",  "arpeggio"};
const MusicParams EmotionMapper::SAD_PARAMS       = { 60, "A", "minor",  60, "melancholic","i-VII-VI-VII","arpeggio"};
const MusicParams EmotionMapper::ANGRY_PARAMS     = {160, "E", "minor", 120, "intense",    "i-iv-V-i",   "block"};
const MusicParams EmotionMapper::SURPRISED_PARAMS = {120, "G", "major",  90, "playful",    "I-vi-IV-V",  "alberti"};
const MusicParams EmotionMapper::NEUTRAL_PARAMS   = { 90, "C", "major",  70, "calm",       "I-IV-V-I",   "arpeggio"};
const MusicParams EmotionMapper::FEAR_PARAMS      = {100, "B", "minor",  80, "tense",      "i-VI-III-VII","arpeggio"};
const MusicParams EmotionMapper::DISGUST_PARAMS   = { 80, "F", "blues",  50, "dark",       "i-iv-i-iv",  "block"};

MusicParams EmotionMapper::mapToMusic(Emotion emotion) {
    switch (emotion) {
        case Emotion::HAPPY:     return HAPPY_PARAMS;
        case Emotion::SAD:       return SAD_PARAMS;
        case Emotion::ANGRY:     return ANGRY_PARAMS;
        case Emotion::SURPRISED: return SURPRISED_PARAMS;
        case Emotion::NEUTRAL:   return NEUTRAL_PARAMS;
        case Emotion::FEAR:      return FEAR_PARAMS;
        case Emotion::DISGUST:   return DISGUST_PARAMS;
        default:                 return NEUTRAL_PARAMS;
    }
}
