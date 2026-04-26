#include "audio_player.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AudioPlayer::AudioPlayer()
    : initialized_(false)
    , playing_(false)
    , child_pid_(-1)
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

    // ADSR 包络参数
    double attack  = std::min(0.02, duration * 0.1);   // 起音
    double decay   = std::min(0.05, duration * 0.15);   // 衰减
    double sustain_level = 0.7;                          // 持续电平
    double release = std::min(0.08, duration * 0.25);    // 释音

    // 泛音系数 (模拟类钢琴音色)
    // 基频=1.0, 2次谐波=0.5, 3次=0.25, 4次=0.12
    static const double harm_amp[] = {1.0, 0.5, 0.25, 0.12};
    static const int num_harmonics = 4;

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // ADSR 包络
        double envelope = 1.0;
        if (t < attack) {
            envelope = t / attack;
        } else if (t < attack + decay) {
            double decay_progress = (t - attack) / decay;
            envelope = 1.0 - (1.0 - sustain_level) * decay_progress;
        } else if (t > duration - release) {
            envelope = sustain_level * (duration - t) / release;
            if (envelope < 0) envelope = 0;
        } else {
            envelope = sustain_level;
        }

        // 叠加泛音
        double sample = 0.0;
        for (int h = 0; h < num_harmonics; ++h) {
            sample += harm_amp[h] * std::sin(2.0 * M_PI * freq * (h + 1) * t);
        }

        samples[i] = static_cast<float>(envelope * 0.4 * sample);
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
    if (std::system("command -v paplay > /dev/null 2>&1") == 0) {
        return "paplay";
    }
    if (std::system("command -v aplay > /dev/null 2>&1") == 0) {
        return "aplay";
    }
    return "";
}

void AudioPlayer::play(const std::vector<Note>& notes) {
    if (!initialized_) {
        if (!init()) {
            std::cerr << "[AudioPlayer] 初始化失败，无法播放" << std::endl;
            return;
        }
    }

    if (notes.empty()) {
        std::cerr << "[AudioPlayer] 没有音符可播放" << std::endl;
        return;
    }

    // 如果上一次还在播放，先停止
    if (child_pid_ > 0) {
        stop();
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

    // 验证 WAV 文件
    struct stat st;
    if (stat(wav_path.c_str(), &st) == 0) {
        std::cout << "[AudioPlayer] WAV 文件: " << wav_path
                  << " (" << st.st_size << " 字节)" << std::endl;
    } else {
        std::cerr << "[AudioPlayer] WAV 文件不存在，写入可能失败" << std::endl;
        return;
    }

    // fork 子进程异步播放，不阻塞主线程
    std::string player_cmd = findPlayerCommand();
    pid_t pid = fork();

    if (pid == 0) {
        // 子进程：直接 exec 播放器，不经过 shell
        execlp(player_cmd.c_str(), player_cmd.c_str(), wav_path.c_str(), (char*)nullptr);
        // 如果 exec 失败
        std::cerr << "[AudioPlayer] execlp 失败: " << player_cmd << std::endl;
        _exit(127);
    } else if (pid > 0) {
        // 父进程：继续主循环，不阻塞
        child_pid_ = pid;
        playing_ = true;
        std::cout << "[AudioPlayer] 开始播放 (PID: " << pid
                  << ", 播放器: " << player_cmd << ")" << std::endl;
    } else {
        std::cerr << "[AudioPlayer] fork() 失败" << std::endl;
    }
}

bool AudioPlayer::isPlaying() {
    if (child_pid_ > 0) {
        int status;
        pid_t ret = waitpid(child_pid_, &status, WNOHANG);
        if (ret == 0) {
            return true;  // 还在播放中
        }
        // 子进程已结束
        child_pid_ = -1;
        playing_ = false;
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::cerr << "[AudioPlayer] 播放器退出码: " << WEXITSTATUS(status)
                      << " (播放器可能未正确配置)" << std::endl;
        } else {
            std::cout << "[AudioPlayer] 播放完成" << std::endl;
        }
    }
    return false;
}

void AudioPlayer::stop() {
    if (child_pid_ > 0) {
        kill(child_pid_, SIGTERM);
        int status;
        waitpid(child_pid_, &status, 0);  // 回收子进程
        child_pid_ = -1;
    }
    std::system("killall paplay 2>/dev/null");
    std::system("killall aplay 2>/dev/null");
    playing_ = false;
}

void AudioPlayer::cleanup() {
    stop();
    initialized_ = false;
}
