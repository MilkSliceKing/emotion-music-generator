#include "audio_player.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AudioPlayer::AudioPlayer()
    : initialized_(false)
    , playing_(false)
{
}

AudioPlayer::~AudioPlayer() {
    cleanup();
}

bool AudioPlayer::init() {
    std::string cmd = findPlayerCommand();
    if (cmd.empty()) {
        std::cerr << "[AudioPlayer] 未找到可用的音频播放器 (paplay/aplay)" << std::endl;
        return false;
    }
    initialized_ = true;
    std::cout << "[AudioPlayer] 初始化成功，使用: " << cmd << std::endl;
    return true;
}

double AudioPlayer::midiToFrequency(int pitch) {
    // A4 (MIDI 69) = 440Hz
    return 440.0 * std::pow(2.0, (pitch - 69) / 12.0);
}

std::vector<float> AudioPlayer::generateSineWave(int pitch, double duration, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    double attack  = 0.01;  // 起音 10ms
    double release = 0.05;  // 释音 50ms

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // 简单包络 (Attack + Sustain + Release)
        double envelope = 1.0;
        if (t < attack) {
            envelope = t / attack;
        } else if (t > duration - release) {
            envelope = (duration - t) / release;
            if (envelope < 0) envelope = 0;
        }

        samples[i] = static_cast<float>(envelope * 0.3 * std::sin(2.0 * M_PI * freq * t));
    }

    return samples;
}

bool AudioPlayer::writeWavFile(const std::string& filename,
                                const std::vector<float>& samples,
                                int sample_rate) {
    // 将 float [-1.0, 1.0] 转为 16-bit PCM
    int num_samples = static_cast<int>(samples.size());
    std::vector<int16_t> pcm_data(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float clamped = std::max(-1.0f, std::min(1.0f, samples[i]));
        pcm_data[i] = static_cast<int16_t>(clamped * 32767.0f);
    }

    int data_size = num_samples * 2;  // 16-bit = 2 bytes per sample
    int file_size = 36 + data_size;    // RIFF header size

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[AudioPlayer] 无法创建 WAV 文件: " << filename << std::endl;
        return false;
    }

    // WAV 文件头 (44 bytes)
    uint8_t header[44];

    // RIFF 标识
    std::memcpy(header, "RIFF", 4);
    uint32_t chunk_size = file_size;
    std::memcpy(header + 4, &chunk_size, 4);
    std::memcpy(header + 8, "WAVE", 4);

    // fmt 子块
    std::memcpy(header + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    std::memcpy(header + 16, &fmt_size, 4);
    uint16_t audio_format = 1;    // PCM
    uint16_t num_channels = 1;    // 单声道
    uint32_t sr = sample_rate;
    uint16_t bits_per_sample = 16;
    uint16_t block_align = num_channels * bits_per_sample / 8;
    uint32_t byte_rate = sr * block_align;
    std::memcpy(header + 20, &audio_format, 2);
    std::memcpy(header + 22, &num_channels, 2);
    std::memcpy(header + 24, &sr, 4);
    std::memcpy(header + 28, &byte_rate, 4);
    std::memcpy(header + 32, &block_align, 2);
    std::memcpy(header + 34, &bits_per_sample, 2);

    // data 子块
    std::memcpy(header + 36, "data", 4);
    uint32_t ds = data_size;
    std::memcpy(header + 40, &ds, 4);

    file.write(reinterpret_cast<char*>(header), 44);
    file.write(reinterpret_cast<char*>(pcm_data.data()), data_size);
    file.close();

    return true;
}

std::string AudioPlayer::findPlayerCommand() {
    // 优先用 paplay (PulseAudio)，其次 aplay (ALSA)
    if (std::system("which paplay > /dev/null 2>&1") == 0) {
        return "paplay";
    }
    if (std::system("which aplay > /dev/null 2>&1") == 0) {
        return "aplay";
    }
    return "";
}

void AudioPlayer::play(const std::vector<Note>& notes) {
    if (!initialized_) {
        if (!init()) return;
    }

    if (notes.empty()) {
        std::cerr << "[AudioPlayer] 没有音符可播放" << std::endl;
        return;
    }

    const int sample_rate = 44100;

    // 合成所有音符
    std::vector<float> audio_data;
    for (const auto& note : notes) {
        auto wave = generateSineWave(note.pitch, note.duration, sample_rate);
        audio_data.insert(audio_data.end(), wave.begin(), wave.end());
    }

    double duration = audio_data.size() / static_cast<double>(sample_rate);
    std::cout << "[AudioPlayer] 合成 " << notes.size() << " 个音符, "
              << duration << " 秒" << std::endl;

    // 写入临时 WAV 文件
    std::string wav_path = "/tmp/emotion_music.wav";
    if (!writeWavFile(wav_path, audio_data, sample_rate)) {
        std::cerr << "[AudioPlayer] WAV 文件写入失败" << std::endl;
        return;
    }

    // 用系统播放器播放
    playing_ = true;
    std::string player_cmd = findPlayerCommand();
    std::string cmd = player_cmd + " " + wav_path + " > /dev/null 2>&1";
    int ret = std::system(cmd.c_str());
    playing_ = false;

    if (ret != 0) {
        std::cerr << "[AudioPlayer] 播放失败 (返回值: " << ret << ")" << std::endl;
    } else {
        std::cout << "[AudioPlayer] 播放完成" << std::endl;
    }
}

void AudioPlayer::stop() {
    // 终止正在播放的进程
    std::system("killall paplay 2>/dev/null");
    std::system("killall aplay 2>/dev/null");
    playing_ = false;
}

void AudioPlayer::cleanup() {
    stop();
    initialized_ = false;
}
