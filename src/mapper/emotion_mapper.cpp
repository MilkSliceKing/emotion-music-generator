#include "emotion_mapper.h"

// 情绪 → 音乐参数的映射表
// 枚举值变了：ANGRY=0, DISGUST=1, FEAR=2, HAPPY=3, SAD=4, SURPRISED=5, NEUTRAL=6

const MusicParams EmotionMapper::ANGRY_PARAMS     = {160, "E", "minor", 120, "intense"};
const MusicParams EmotionMapper::DISGUST_PARAMS   = { 80, "F", "blues",  50, "dark"};
const MusicParams EmotionMapper::FEAR_PARAMS      = {100, "B", "minor",  80, "tense"};
const MusicParams EmotionMapper::HAPPY_PARAMS     = {140, "C", "major", 100, "bright"};
const MusicParams EmotionMapper::SAD_PARAMS       = { 60, "A", "minor",  60, "melancholic"};
const MusicParams EmotionMapper::SURPRISED_PARAMS = {120, "G", "major",  90, "playful"};
const MusicParams EmotionMapper::NEUTRAL_PARAMS   = { 90, "C", "major",  70, "calm"};

MusicParams EmotionMapper::mapToMusic(Emotion emotion) {
    switch (emotion) {
        case Emotion::ANGRY:     return ANGRY_PARAMS;
        case Emotion::DISGUST:   return DISGUST_PARAMS;
        case Emotion::FEAR:      return FEAR_PARAMS;
        case Emotion::HAPPY:     return HAPPY_PARAMS;
        case Emotion::SAD:       return SAD_PARAMS;
        case Emotion::SURPRISED: return SURPRISED_PARAMS;
        case Emotion::NEUTRAL:   return NEUTRAL_PARAMS;
        default:                 return NEUTRAL_PARAMS;
    }
}
