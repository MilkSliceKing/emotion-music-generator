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
    return 440.0 * std::pow(2.0, (pitch - 69) / 12.0);
}

Timbre AudioPlayer::moodToTimbre(const std::string& mood) {
    if (mood == "bright")    return Timbre::BRIGHT_PIANO;
    if (mood == "playful")   return Timbre::BRIGHT_PIANO;
    if (mood == "calm")      return Timbre::MELLOW_PIANO;
    if (mood == "melancholic") return Timbre::SOFT_STRINGS;
    if (mood == "tense")     return Timbre::PLUCKED;
    if (mood == "dark")      return Timbre::DARK_PAD;
    if (mood == "intense")   return Timbre::HARSH;
    return Timbre::MELLOW_PIANO;
}

// ========== 各音色合成方法 ==========

std::vector<float> AudioPlayer::synthPiano(int pitch, double duration,
                                            int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    // 力度影响亮度（力度越大泛音越丰富）
    double vel_factor = velocity / 127.0;
    int max_harmonics = static_cast<int>(4 + vel_factor * 4);  // 4~8 个泛音

    // 钢琴特征：快速起音 + 指数衰减（无持续阶段）
    double attack = 0.005;  // 5ms 极短起音
    double decay_rate = 3.0; // 衰减速率，值越大衰减越快

    // 微失谐：两个振荡器略微失谐，增加温暖感
    double detune = 1.001;

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // 包络：快速起音 + 指数衰减
        double envelope;
        if (t < attack) {
            envelope = t / attack;
        } else {
            envelope = std::exp(-decay_rate * (t - attack));
        }
        // 尾部平滑释音
        double tail = 0.05;
        if (t > duration - tail) {
            double fade = (duration - t) / tail;
            envelope *= fade;
        }

        // 叠加泛音（含微失谐和钢琴非整数泛音特征）
        double sample = 0.0;
        for (int h = 1; h <= max_harmonics; ++h) {
            // 钢琴弦的非整数泛音（拉伸调谐）
            double stretch = 1.0 + 0.0002 * h * h;
            double harmonic_freq = freq * h * stretch;

            // 泛音幅度：1/h^1.5，且越高泛音衰减越快
            double amp = 1.0 / std::pow(h, 1.5);

            // 主振荡器
            sample += amp * std::sin(2.0 * M_PI * harmonic_freq * t);
            // 失谐振荡器（增加温暖感）
            sample += amp * 0.3 * std::sin(2.0 * M_PI * harmonic_freq * detune * t);
        }

        samples[i] = static_cast<float>(envelope * 0.25 * sample);
    }

    return samples;
}

std::vector<float> AudioPlayer::synthSoftPiano(int pitch, double duration,
                                                int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    double vel_factor = velocity / 127.0;

    // 柔和钢琴特征：更少的泛音、更慢的起音、更温暖的音色
    int max_harmonics = static_cast<int>(2 + vel_factor * 2);  // 2~4 个泛音（比明亮钢琴少）
    double attack = 0.015;   // 15ms 较柔和的起音（比明亮钢琴慢3倍）
    double decay_rate = 2.0;  // 较慢的衰减，更绵长
    double tail = 0.1;        // 较长的尾部释音

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // 包络：柔和起音 + 较慢指数衰减
        double envelope;
        if (t < attack) {
            // 二次曲线起音，比线性更柔和
            double ratio = t / attack;
            envelope = ratio * ratio;
        } else {
            envelope = std::exp(-decay_rate * (t - attack));
        }
        if (t > duration - tail) {
            double fade = (duration - t) / tail;
            envelope *= fade * fade;  // 平滑释音
        }

        // 泛音：强调基频，高次泛音快速衰减
        double sample = 0.0;
        for (int h = 1; h <= max_harmonics; ++h) {
            // 泛音幅度：1/h^2（比明亮钢琴衰减更快，听起来更圆）
            double amp = 1.0 / static_cast<double>(h * h);

            // 时间相关的高频衰减（模拟琴槌的软质特性）
            double h_decay = std::exp(-0.5 * h * t);
            amp *= h_decay;

            sample += amp * std::sin(2.0 * M_PI * freq * h * t);
        }

        // 轻微的合唱效果（两列非常接近的振荡器），增加温暖感
        double detune1 = 1.0005;
        double detune2 = 0.9995;
        double chorus = 0.0;
        chorus += 0.3 * std::sin(2.0 * M_PI * freq * detune1 * t);
        chorus += 0.3 * std::sin(2.0 * M_PI * freq * detune2 * t);

        samples[i] = static_cast<float>(envelope * 0.2 * (sample + chorus));
    }

    return samples;
}

std::vector<float> AudioPlayer::synthStrings(int pitch, double duration,
                                              int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    double vel_factor = velocity / 127.0;

    // 弦乐特征：慢起音 + 持续 + 颤音
    double attack = std::min(0.15, duration * 0.3);   // 慢起音
    double release = std::min(0.2, duration * 0.3);    // 慢释音
    double vibrato_rate = 5.5;   // 颤音频率 Hz
    double vibrato_depth = 2.0;  // 颤音深度（半音的百分比）

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // ADSR 包络（弦乐风格）
        double envelope = 1.0;
        if (t < attack) {
            envelope = t / attack;
            envelope = envelope * envelope;  // 二次曲线，更柔和
        }
        if (t > duration - release) {
            double fade = (duration - t) / release;
            envelope *= fade * fade;
        }

        // 颤音（起音后逐渐加入）
        double vibrato = 0.0;
        if (t > attack) {
            double vib_env = std::min(1.0, (t - attack) / 0.3);
            vibrato = vibrato_depth * vib_env * std::sin(2.0 * M_PI * vibrato_rate * t);
        }

        double mod_freq = freq * std::pow(2.0, vibrato / 1200.0);

        // 弦乐泛音结构（丰富的奇次谐波）
        double sample = 0.0;
        double amps[] = {1.0, 0.5, 0.35, 0.2, 0.15, 0.08};
        for (int h = 0; h < 6; ++h) {
            double harmonic_freq = mod_freq * (h + 1);
            // 慢起音也要影响泛音
            sample += amps[h] * std::sin(2.0 * M_PI * harmonic_freq * t);
        }

        samples[i] = static_cast<float>(envelope * 0.18 * vel_factor * sample);
    }

    return samples;
}

std::vector<float> AudioPlayer::synthPad(int pitch, double duration,
                                          int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    double vel_factor = velocity / 127.0;

    // 铺底音色：非常慢的起音 + 柔和的泛音
    double attack = std::min(0.3, duration * 0.4);
    double release = std::min(0.3, duration * 0.4);

    // LFO 调制（缓慢的音量起伏）
    double lfo_rate = 0.8;

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // 慢起音
        double envelope = 1.0;
        if (t < attack) {
            envelope = t / attack;
            envelope = envelope * envelope * envelope;  // 三次曲线
        }
        if (t > duration - release) {
            double fade = (duration - t) / release;
            envelope *= fade * fade * fade;
        }

        // LFO 调制音量
        double lfo = 0.85 + 0.15 * std::sin(2.0 * M_PI * lfo_rate * t);
        envelope *= lfo;

        // 柔和泛音（只有少量，且幅度小）
        double sample = 0.0;
        sample += 1.0  * std::sin(2.0 * M_PI * freq * t);
        sample += 0.4  * std::sin(2.0 * M_PI * freq * 2.0 * t);
        sample += 0.15 * std::sin(2.0 * M_PI * freq * 3.0 * t);
        // 加入一点失谐增加空间感
        sample += 0.25 * std::sin(2.0 * M_PI * freq * 1.003 * t);

        samples[i] = static_cast<float>(envelope * 0.2 * vel_factor * sample);
    }

    return samples;
}

std::vector<float> AudioPlayer::synthPlucked(int pitch, double duration,
                                              int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    double vel_factor = velocity / 127.0;

    // 拨弦特征：极短起音 + 快速衰减（比钢琴更亮的初始音）
    double attack = 0.002;
    double decay_rate = 4.5;

    // Karplus-Strong 简化版：混合锯齿波的高频成分
    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // 包络
        double envelope;
        if (t < attack) {
            envelope = t / attack;
        } else {
            // 快速指数衰减，高频衰减更快
            envelope = std::exp(-decay_rate * (t - attack));
        }
        double tail = 0.03;
        if (t > duration - tail) {
            envelope *= (duration - t) / tail;
        }

        // 拨弦音色：丰富的高次谐波（类似锯齿波但更柔和）
        double sample = 0.0;
        int max_h = static_cast<int>(8 + vel_factor * 4);
        for (int h = 1; h <= max_h; ++h) {
            // 偶次谐波较弱（类似吉他弦）
            double amp = (h % 2 == 1) ? 1.0 / h : 0.5 / h;
            // 高频比低频衰减更快
            double h_decay = std::exp(-0.3 * h * t);
            sample += amp * h_decay * std::sin(2.0 * M_PI * freq * h * t);
        }

        samples[i] = static_cast<float>(envelope * 0.22 * vel_factor * sample);
    }

    return samples;
}

std::vector<float> AudioPlayer::synthHarsh(int pitch, double duration,
                                            int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    double vel_factor = velocity / 127.0;

    // 粗犷音色：快起音 + 锯齿波特征 + 失真感
    double attack = 0.003;
    double sustain_level = 0.8;

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // 包络
        double envelope = 1.0;
        if (t < attack) {
            envelope = t / attack;
        }
        double release = 0.04;
        if (t > duration - release) {
            envelope *= (duration - t) / release;
        }

        // 锯齿波（丰富的所有谐波，听起来更"硬"）
        double saw = 0.0;
        for (int h = 1; h <= 12; ++h) {
            double amp = 1.0 / h;
            saw += amp * std::sin(2.0 * M_PI * freq * h * t);
        }

        // 轻微失真（软削波）
        double sample = std::tanh(1.5 * saw);

        // 加入轻微噪声增加"毛刺感"
        double noise = (std::rand() / (double)RAND_MAX - 0.5) * 0.05;
        sample += noise;

        samples[i] = static_cast<float>(envelope * sustain_level * 0.2 * vel_factor * sample);
    }

    return samples;
}

// ========== 统一接口 ==========

std::vector<float> AudioPlayer::generateNote(int pitch, double duration,
                                              Timbre timbre, int velocity,
                                              int sample_rate) {
    switch (timbre) {
        case Timbre::BRIGHT_PIANO:
            return synthPiano(pitch, duration, velocity, sample_rate);
        case Timbre::MELLOW_PIANO:
            return synthSoftPiano(pitch, duration, velocity, sample_rate);
        case Timbre::SOFT_STRINGS:
            return synthStrings(pitch, duration, velocity, sample_rate);
        case Timbre::DARK_PAD:
            return synthPad(pitch, duration, velocity, sample_rate);
        case Timbre::PLUCKED:
            return synthPlucked(pitch, duration, velocity, sample_rate);
        case Timbre::HARSH:
            return synthHarsh(pitch, duration, velocity, sample_rate);
        default:
            return synthPiano(pitch, duration, velocity, sample_rate);
    }
}

// ========== WAV / 播放 ==========

void AudioPlayer::playComposition(const Composition& comp) {
    if (!initialized_) {
        if (!init()) {
            std::cerr << "[AudioPlayer] 初始化失败，无法播放" << std::endl;
            return;
        }
    }

    if (child_pid_ > 0) {
        stop();
    }

    const int sample_rate = 44100;
    Timbre timbre = moodToTimbre(comp.mood);

    // 计算总采样数
    int total_samples = static_cast<int>(comp.total_duration * sample_rate);
    if (total_samples <= 0) {
        std::cerr << "[AudioPlayer] 乐曲时长为空" << std::endl;
        return;
    }

    // 分配混合缓冲区
    std::vector<float> mix_buffer(total_samples, 0.0f);

    // 渲染旋律（使用情绪对应音色）
    int melody_count = 0, acc_count = 0;
    for (const auto& note : comp.melody) {
        if (note.is_rest) continue;
        auto wave = generateNote(note.pitch, note.duration, timbre,
                                  note.velocity, sample_rate);
        int start_sample = static_cast<int>(note.start_time * sample_rate);
        // 叠加到混合缓冲区
        for (size_t i = 0; i < wave.size(); ++i) {
            int idx = start_sample + static_cast<int>(i);
            if (idx >= 0 && idx < total_samples) {
                mix_buffer[idx] += wave[i];
            }
        }
        melody_count++;
    }

    // 渲染伴奏（使用柔和钢琴，力度已在生成时降低）
    Timbre acc_timbre = Timbre::MELLOW_PIANO;
    for (const auto& note : comp.accompaniment) {
        if (note.is_rest) continue;
        auto wave = generateNote(note.pitch, note.duration, acc_timbre,
                                  note.velocity, sample_rate);
        int start_sample = static_cast<int>(note.start_time * sample_rate);
        for (size_t i = 0; i < wave.size(); ++i) {
            int idx = start_sample + static_cast<int>(i);
            if (idx >= 0 && idx < total_samples) {
                mix_buffer[idx] += wave[i];
            }
        }
        acc_count++;
    }

    // 全局归一化防削波
    float peak = 0.0f;
    for (float s : mix_buffer) {
        float abs_s = std::abs(s);
        if (abs_s > peak) peak = abs_s;
    }
    if (peak > 0.9f) {
        float scale = 0.9f / peak;
        for (float& s : mix_buffer) {
            s *= scale;
        }
    }

    double duration = mix_buffer.size() / static_cast<double>(sample_rate);
    std::cout << "[AudioPlayer] 合成乐曲: " << melody_count << " 个旋律音符, "
              << acc_count << " 个伴奏音符, " << duration << " 秒, 音色: " << comp.mood
              << std::endl;

    std::string wav_path = "/tmp/emotion_music.wav";
    if (!writeWavFile(wav_path, mix_buffer, sample_rate)) {
        std::cerr << "[AudioPlayer] WAV 文件写入失败" << std::endl;
        return;
    }

    struct stat st;
    if (stat(wav_path.c_str(), &st) == 0) {
        std::cout << "[AudioPlayer] WAV 文件: " << wav_path
                  << " (" << st.st_size << " 字节)" << std::endl;
    } else {
        std::cerr << "[AudioPlayer] WAV 文件不存在，写入可能失败" << std::endl;
        return;
    }

    std::string player_cmd = findPlayerCommand();
    pid_t pid = fork();

    if (pid == 0) {
        execlp(player_cmd.c_str(), player_cmd.c_str(), wav_path.c_str(), (char*)nullptr);
        std::cerr << "[AudioPlayer] execlp 失败: " << player_cmd << std::endl;
        _exit(127);
    } else if (pid > 0) {
        child_pid_ = pid;
        playing_ = true;
        std::cout << "[AudioPlayer] 开始播放 (PID: " << pid
                  << ", 播放器: " << player_cmd << ")" << std::endl;
    } else {
        std::cerr << "[AudioPlayer] fork() 失败" << std::endl;
    }
}

bool AudioPlayer::writeWavFile(const std::string& filename,
                                const std::vector<float>& samples,
                                int sample_rate) {
    int num_samples = static_cast<int>(samples.size());
    std::vector<int16_t> pcm_data(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float clamped = std::max(-1.0f, std::min(1.0f, samples[i]));
        pcm_data[i] = static_cast<int16_t>(clamped * 32767.0f);
    }

    int data_size = num_samples * 2;
    int file_size = 36 + data_size;

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[AudioPlayer] 无法创建 WAV 文件: " << filename << std::endl;
        return false;
    }

    uint8_t header[44];
    std::memcpy(header, "RIFF", 4);
    uint32_t chunk_size = file_size;
    std::memcpy(header + 4, &chunk_size, 4);
    std::memcpy(header + 8, "WAVE", 4);
    std::memcpy(header + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    std::memcpy(header + 16, &fmt_size, 4);
    uint16_t audio_format = 1;
    uint16_t num_channels = 1;
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
    std::memcpy(header + 36, "data", 4);
    uint32_t ds = data_size;
    std::memcpy(header + 40, &ds, 4);

    file.write(reinterpret_cast<char*>(header), 44);
    file.write(reinterpret_cast<char*>(pcm_data.data()), data_size);
    file.close();

    return true;
}

std::string AudioPlayer::findPlayerCommand() {
    if (std::system("command -v paplay > /dev/null 2>&1") == 0) {
        return "paplay";
    }
    if (std::system("command -v aplay > /dev/null 2>&1") == 0) {
        return "aplay";
    }
    return "";
}

void AudioPlayer::play(const std::vector<Note>& notes, const std::string& mood) {
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

    if (child_pid_ > 0) {
        stop();
    }

    const int sample_rate = 44100;
    Timbre timbre = moodToTimbre(mood);

    // 合成所有音符
    std::vector<float> audio_data;
    for (const auto& note : notes) {
        auto wave = generateNote(note.pitch, note.duration, timbre, note.velocity, sample_rate);
        audio_data.insert(audio_data.end(), wave.begin(), wave.end());
    }

    double duration = audio_data.size() / static_cast<double>(sample_rate);
    std::cout << "[AudioPlayer] 合成 " << notes.size() << " 个音符, "
              << duration << " 秒, 音色: " << mood << std::endl;

    std::string wav_path = "/tmp/emotion_music.wav";
    if (!writeWavFile(wav_path, audio_data, sample_rate)) {
        std::cerr << "[AudioPlayer] WAV 文件写入失败" << std::endl;
        return;
    }

    struct stat st;
    if (stat(wav_path.c_str(), &st) == 0) {
        std::cout << "[AudioPlayer] WAV 文件: " << wav_path
                  << " (" << st.st_size << " 字节)" << std::endl;
    } else {
        std::cerr << "[AudioPlayer] WAV 文件不存在，写入可能失败" << std::endl;
        return;
    }

    std::string player_cmd = findPlayerCommand();
    pid_t pid = fork();

    if (pid == 0) {
        execlp(player_cmd.c_str(), player_cmd.c_str(), wav_path.c_str(), (char*)nullptr);
        std::cerr << "[AudioPlayer] execlp 失败: " << player_cmd << std::endl;
        _exit(127);
    } else if (pid > 0) {
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
            return true;
        }
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
        waitpid(child_pid_, &status, 0);
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
