#include "emotion_mapper.h"

// 情绪 → 音乐参数的映射表
// FER+ 枚举: NEUTRAL=0, HAPPY=1, SURPRISED=2, SAD=3, ANGRY=4, DISGUST=5, FEAR=6
//
// 和弦进行说明（度数基于自然音阶）:
//   Happy:   I-V-vi-IV  (C-G-Am-F, 流行阳光进行)
//   Sad:     i-VII-VI-VII (Am-G-F-G, 安达卢西亚悲伤进行)
//   Angry:   i-iv-V-i   (Em-Am-Bm-Em, 强力小调)
//   Surprised: I-IV-V-I (G-C-D-G, 号角般明亮)
//   Neutral: I-vi-IV-V  (C-Am-F-G, 50年代经典)
//   Fear:    i-iv-V-i   (Bm-Em-F#m-Bm, 紧张小调)
//   Disgust: i-VI-V-iv  (Fm-Db-C-Bbm, 暗沉下行)

const MusicParams EmotionMapper::HAPPY_PARAMS     = {140, "C", "major", 100, "bright",    {0,4,5,3}, "arpeggiated", 2};
const MusicParams EmotionMapper::SAD_PARAMS       = { 60, "A", "minor",  60, "melancholic",{0,6,5,6}, "block",       2};
const MusicParams EmotionMapper::ANGRY_PARAMS     = {160, "E", "minor", 120, "intense",    {0,3,4,0}, "block",       2};
const MusicParams EmotionMapper::SURPRISED_PARAMS = {120, "G", "major",  90, "playful",    {0,3,4,0}, "arpeggiated", 2};
const MusicParams EmotionMapper::NEUTRAL_PARAMS   = { 90, "C", "major",  70, "calm",       {0,5,3,4}, "alberti",     2};
const MusicParams EmotionMapper::FEAR_PARAMS      = {100, "B", "minor",  80, "tense",      {0,3,4,0}, "arpeggiated", 2};
const MusicParams EmotionMapper::DISGUST_PARAMS   = { 80, "F", "blues",  50, "dark",       {0,5,4,3}, "alberti",     2};

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
