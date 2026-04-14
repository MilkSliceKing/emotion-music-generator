#include "audio_player.h"

#ifdef HAS_PORTAUDIO
#include <portaudio.h>
#include <cmath>
#include <iostream>

AudioPlayer::AudioPlayer()
    : initialized_(false)
    , stream_(nullptr)
{
}

AudioPlayer::~AudioPlayer() {
    cleanup();
}

bool AudioPlayer::init() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "[AudioPlayer] PortAudio 初始化失败: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    initialized_ = true;
    std::cout << "[AudioPlayer] PortAudio 初始化成功" << std::endl;
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

        // 简单 ADSR 包络 (只用 Attack + Sustain + Release)
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

    std::cout << "[AudioPlayer] 合成 " << notes.size() << " 个音符, "
              << audio_data.size() << " 个采样点, "
              << audio_data.size() / static_cast<double>(sample_rate) << " 秒" << std::endl;

    // 配置输出参数
    PaStreamParameters output_params;
    output_params.device = Pa_GetDefaultOutputDevice();

    // 打印设备信息
    std::cout << "[AudioPlayer] 默认输出设备ID: " << output_params.device << std::endl;
    if (output_params.device != paNoDevice) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(output_params.device);
        std::cout << "[AudioPlayer] 设备名: " << (info ? info->name : "null") << std::endl;
        std::cout << "[AudioPlayer] Host API: " << Pa_GetHostApiInfo(info->hostApi)->name << std::endl;
    }

    if (output_params.device == paNoDevice) {
        std::cerr << "[AudioPlayer] 未找到音频输出设备" << std::endl;
        return;
    }
    output_params.channelCount = 1;
    output_params.sampleFormat = paFloat32;
    output_params.suggestedLatency = Pa_GetDeviceInfo(output_params.device)->defaultLowOutputLatency;
    output_params.hostApiSpecificStreamInfo = nullptr;

    // 打开音频流
    PaError err = Pa_OpenStream(
        reinterpret_cast<PaStream**>(&stream_),
        nullptr,
        &output_params,
        sample_rate,
        audio_data.size(),
        paClipOff,
        nullptr,
        nullptr
    );

    if (err != paNoError) {
        std::cerr << "[AudioPlayer] 打开音频流失败: " << Pa_GetErrorText(err) << std::endl;
        return;
    }
    std::cout << "[AudioPlayer] 音频流已打开" << std::endl;

    // 播放
    err = Pa_StartStream(reinterpret_cast<PaStream*>(stream_));
    if (err != paNoError) {
        std::cerr << "[AudioPlayer] 启动音频流失败: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(reinterpret_cast<PaStream*>(stream_));
        stream_ = nullptr;
        return;
    }
    std::cout << "[AudioPlayer] 开始播放..." << std::endl;

    err = Pa_WriteStream(reinterpret_cast<PaStream*>(stream_), audio_data.data(), audio_data.size());
    if (err != paNoError) {
        std::cerr << "[AudioPlayer] 写入音频数据失败: " << Pa_GetErrorText(err) << std::endl;
    } else {
        std::cout << "[AudioPlayer] 播放完成" << std::endl;
    }

    stop();
}

void AudioPlayer::stop() {
    if (stream_) {
        Pa_StopStream(reinterpret_cast<PaStream*>(stream_));
        Pa_CloseStream(reinterpret_cast<PaStream*>(stream_));
        stream_ = nullptr;
    }
}

void AudioPlayer::cleanup() {
    stop();
    if (initialized_) {
        Pa_Terminate();
        initialized_ = false;
    }
}

#else
// PortAudio 未安装时的桩实现
AudioPlayer::AudioPlayer() : initialized_(false), stream_(nullptr) {}
AudioPlayer::~AudioPlayer() {}
bool AudioPlayer::init() { return false; }
void AudioPlayer::play(const std::vector<Note>&) {
    std::cerr << "[AudioPlayer] PortAudio 未安装，音频功能不可用" << std::endl;
}
void AudioPlayer::stop() {}
void AudioPlayer::cleanup() {}
double AudioPlayer::midiToFrequency(int) { return 0; }
std::vector<float> AudioPlayer::generateSineWave(int, double, int) { return {}; }
#endif
