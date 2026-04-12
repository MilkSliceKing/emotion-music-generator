#ifndef EMOTION_MAPPER_H
#define EMOTION_MAPPER_H

#include <string>
#include "detector/emotion_recognizer.h"

// 音乐参数结构体
struct MusicParams {
    int tempo;          // BPM
    std::string key;    // 调性: C, D, E, F, G, A, B
    std::string scale;  // 音阶: major, minor, blues
    int velocity;       // MIDI 力度 0-127
    std::string mood;   // 情绪标签
};

class EmotionMapper {
public:
    EmotionMapper() = default;
    ~EmotionMapper() = default;

    MusicParams mapToMusic(Emotion emotion);

private:
    static const MusicParams HAPPY_PARAMS;
    static const MusicParams SAD_PARAMS;
    static const MusicParams ANGRY_PARAMS;
    static const MusicParams SURPRISED_PARAMS;
    static const MusicParams NEUTRAL_PARAMS;
    static const MusicParams FEAR_PARAMS;
    static const MusicParams DISGUST_PARAMS;
};

#endif // EMOTION_MAPPER_H
