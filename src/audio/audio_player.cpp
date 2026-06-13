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

// ========== 攻击瞬态 ==========

std::vector<float> AudioPlayer::generateNoiseBurst(int num_samples, double amplitude,
                                                     double decay_rate) {
    std::vector<float> burst(num_samples, 0.0f);
    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / 44100.0;
        double env = amplitude * std::exp(-decay_rate * t);
        burst[i] = static_cast<float>(env * (std::rand() / (double)RAND_MAX - 0.5));
    }
    return burst;
}

// ========== 各音色合成方法 ==========

std::vector<float> AudioPlayer::synthPiano(int pitch, double duration,
                                            int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples, 0.0f);

    double vel_factor = velocity / 127.0;
    double vel_curve = std::pow(vel_factor, 1.5);
    int max_harmonics = static_cast<int>(6 + vel_factor * 6);  // 6~12 泛音

    // ---- 真钢琴包络：双段衰减 ----
    // 阶段1：快速初衰减（琴槌冲击能量的快速消散）
    // 阶段2：慢速长尾（弦的持续振动）
    double attack = 0.003;           // 3ms 极短起音
    double fast_decay = 8.0;         // 初衰减速率（快）
    double fast_end = 0.08;          // 初衰减持续 80ms
    double slow_decay = 0.8;         // 长尾衰减速率（慢）
    double sustain_level = 0.35;     // 初衰减结束后的持续电平

    // 低音弦更长、衰减更慢；高音弦更短、衰减更快
    double pitch_factor = std::max(0.5, std::min(2.0, freq / 261.6));  // 相对 C4
    slow_decay *= pitch_factor;
    fast_decay *= pitch_factor;

    // ---- 三弦模拟（3 根弦微失谐）----
    double detunes[3] = {0.9994, 1.0, 1.0006};  // 中弦准确，左右弦微偏
    double detune_amps[3] = {0.85, 1.0, 0.85};  // 中弦稍响

    // ---- 攻击瞬态：击弦噪声 ----
    int burst_samples = static_cast<int>(0.004 * sample_rate);
    auto burst = generateNoiseBurst(burst_samples, 0.12 * vel_curve, 60.0);
    // 将噪声限制在基频附近（模拟锤头击弦的频谱特征）
    for (int i = 0; i < burst_samples && i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;
        // 用基频调制噪声，使其集中在低频
        double mod = 0.5 + 0.5 * std::sin(2.0 * M_PI * freq * 0.5 * t);
        samples[i] = burst[i] * mod;
    }

    // ---- 琴体共振：基频的二次谐波附近有一个共振峰 ----
    double body_res_freq = freq * 2.01;  // 略高于二次谐波
    double body_res_amp = 0.08;

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // 双段衰减包络
        double envelope;
        if (t < attack) {
            double x = t / attack;
            envelope = x * (2.0 - x);  // 微过冲起音
        } else if (t < attack + fast_end) {
            // 快速初衰减：从 1.0 降到 sustain_level
            double progress = (t - attack) / fast_end;
            envelope = sustain_level + (1.0 - sustain_level) * std::exp(-fast_decay * progress);
        } else {
            // 慢速长尾
            double dt = t - attack - fast_end;
            envelope = sustain_level * std::exp(-slow_decay * dt);
        }
        // 尾部平滑释音
        double tail = 0.08;
        if (t > duration - tail) {
            double fade = (duration - t) / tail;
            envelope *= fade * fade;
        }

        // 三弦叠加
        double sample = 0.0;
        for (int s = 0; s < 3; ++s) {
            double string_sample = 0.0;
            for (int h = 1; h <= max_harmonics; ++h) {
                // 钢琴弦非整数泛音（拉伸调谐，低音更明显）
                double inharmonicity = 1.0 + 0.00015 * h * h * (1.0 + 2.0 / pitch_factor);
                double harmonic_freq = freq * h * inharmonicity * detunes[s];

                // 力度影响亮度
                double bright = 1.0 / std::pow(h, 1.1);
                double soft = 1.0 / std::pow(h, 2.0);
                double amp = vel_factor * bright + (1.0 - vel_factor) * soft;

                // 高频时间衰减（高泛音衰减更快）
                double h_decay = std::exp(-0.3 * h * t);

                string_sample += amp * h_decay * std::sin(2.0 * M_PI * harmonic_freq * t);
            }
            sample += detune_amps[s] * string_sample;
        }

        // 琴体共振
        sample += body_res_amp * std::sin(2.0 * M_PI * body_res_freq * t) * envelope;

        samples[i] += static_cast<float>(envelope * 0.18 * vel_curve * sample);
    }

    return samples;
}

std::vector<float> AudioPlayer::synthStrings(int pitch, double duration,
                                              int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    double vel_factor = velocity / 127.0;
    double vel_curve = std::pow(vel_factor, 1.5);

    double attack = std::min(0.15, duration * 0.3);
    double release = std::min(0.2, duration * 0.3);
    double vibrato_rate = 5.5;
    double vibrato_depth = 2.0;

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        double envelope = 1.0;
        if (t < attack) {
            double x = t / attack;
            envelope = x * x * (1.0 + 0.1 * (1.0 - x));
        }
        if (t > duration - release) {
            double fade = (duration - t) / release;
            envelope *= fade * fade;
        }

        double vibrato = 0.0;
        if (t > attack) {
            double vib_env = std::min(1.0, (t - attack) / 0.3);
            vibrato = vibrato_depth * vib_env * std::sin(2.0 * M_PI * vibrato_rate * t);
        }

        double mod_freq = freq * std::pow(2.0, vibrato / 1200.0);

        double sample = 0.0;
        double amps[] = {1.0, 0.5, 0.35, 0.2, 0.15, 0.08};
        for (int h = 0; h < 6; ++h) {
            double harmonic_freq = mod_freq * (h + 1);
            sample += amps[h] * std::sin(2.0 * M_PI * harmonic_freq * t);
        }

        // 弓弦噪声：持续低幅度噪声
        double bow_noise = 0.03 * vel_curve * (std::rand() / (double)RAND_MAX - 0.5);

        samples[i] = static_cast<float>(envelope * 0.18 * vel_curve * sample + envelope * bow_noise);
    }

    return samples;
}

std::vector<float> AudioPlayer::synthPad(int pitch, double duration,
                                          int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    double vel_factor = velocity / 127.0;
    double vel_curve = std::pow(vel_factor, 1.5);

    double attack = std::min(0.3, duration * 0.4);
    double release = std::min(0.3, duration * 0.4);
    double lfo_rate = 0.8;

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        double envelope = 1.0;
        if (t < attack) {
            envelope = t / attack;
            envelope = envelope * envelope * envelope;
        }
        if (t > duration - release) {
            double fade = (duration - t) / release;
            envelope *= fade * fade * fade;
        }

        double lfo = 0.85 + 0.15 * std::sin(2.0 * M_PI * lfo_rate * t);
        envelope *= lfo;

        double sample = 0.0;
        sample += 1.0  * std::sin(2.0 * M_PI * freq * t);
        sample += 0.4  * std::sin(2.0 * M_PI * freq * 2.0 * t);
        sample += 0.15 * std::sin(2.0 * M_PI * freq * 3.0 * t);
        sample += 0.25 * std::sin(2.0 * M_PI * freq * 1.003 * t);

        samples[i] = static_cast<float>(envelope * 0.2 * vel_curve * sample);
    }

    return samples;
}

std::vector<float> AudioPlayer::synthPlucked(int pitch, double duration,
                                              int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples, 0.0f);

    double vel_factor = velocity / 127.0;
    double vel_curve = std::pow(vel_factor, 1.5);

    double attack = 0.002;
    double decay_rate = 4.5;

    // 攻击瞬态：2ms 噪声冲击
    int burst_samples = static_cast<int>(0.002 * sample_rate);
    auto burst = generateNoiseBurst(burst_samples, 0.20 * vel_curve, 60.0);
    for (int i = 0; i < burst_samples && i < num_samples; ++i) {
        samples[i] = burst[i];
    }

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        double envelope;
        if (t < attack) {
            envelope = t / attack;
        } else {
            envelope = std::exp(-decay_rate * (t - attack));
        }
        double tail = 0.03;
        if (t > duration - tail) {
            envelope *= (duration - t) / tail;
        }

        double sample = 0.0;
        int max_h = static_cast<int>(8 + vel_factor * 4);
        for (int h = 1; h <= max_h; ++h) {
            double amp = (h % 2 == 1) ? 1.0 / h : 0.5 / h;
            double h_decay = std::exp(-0.3 * h * t);
            sample += amp * h_decay * std::sin(2.0 * M_PI * freq * h * t);
        }

        samples[i] += static_cast<float>(envelope * 0.22 * vel_curve * sample);
    }

    return samples;
}

std::vector<float> AudioPlayer::synthHarsh(int pitch, double duration,
                                            int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples);

    double vel_factor = velocity / 127.0;
    double vel_curve = std::pow(vel_factor, 1.5);

    double attack = 0.003;
    double sustain_level = 0.8;

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        double envelope = 1.0;
        if (t < attack) {
            envelope = t / attack;
        }
        double release = 0.04;
        if (t > duration - release) {
            envelope *= (duration - t) / release;
        }

        double saw = 0.0;
        for (int h = 1; h <= 12; ++h) {
            double amp = 1.0 / h;
            saw += amp * std::sin(2.0 * M_PI * freq * h * t);
        }

        double sample = std::tanh(1.5 * saw);
        double noise = (std::rand() / (double)RAND_MAX - 0.5) * 0.05;
        sample += noise;

        samples[i] = static_cast<float>(envelope * sustain_level * 0.2 * vel_curve * sample);
    }

    return samples;
}

std::vector<float> AudioPlayer::synthSoftPiano(int pitch, double duration,
                                                int velocity, int sample_rate) {
    double freq = midiToFrequency(pitch);
    int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<float> samples(num_samples, 0.0f);

    double vel_factor = velocity / 127.0;
    double vel_curve = std::pow(vel_factor, 1.5);
    int max_harmonics = static_cast<int>(4 + vel_factor * 4);  // 4~8 泛音

    // ---- 柔和钢琴包络：更慢的双段衰减 ----
    double attack = 0.012;
    double fast_decay = 5.0;
    double fast_end = 0.12;
    double slow_decay = 0.5;
    double sustain_level = 0.40;     // 柔和钢琴持续电平更高

    double pitch_factor = std::max(0.5, std::min(2.0, freq / 261.6));
    slow_decay *= pitch_factor;
    fast_decay *= pitch_factor;

    // 三弦模拟（失谐更小，更温暖）
    double detunes[3] = {0.9997, 1.0, 1.0003};
    double detune_amps[3] = {0.9, 1.0, 0.9};

    // 柔和噪声冲击
    int burst_samples = static_cast<int>(0.006 * sample_rate);
    auto burst = generateNoiseBurst(burst_samples, 0.06 * vel_curve, 35.0);
    for (int i = 0; i < burst_samples && i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;
        double mod = 0.5 + 0.5 * std::sin(2.0 * M_PI * freq * 0.3 * t);
        samples[i] = burst[i] * mod;
    }

    // 琴体共振
    double body_res_freq = freq * 2.005;
    double body_res_amp = 0.05;

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;

        // 双段衰减
        double envelope;
        if (t < attack) {
            double x = t / attack;
            envelope = x * x;  // 二次曲线，更柔和
        } else if (t < attack + fast_end) {
            double progress = (t - attack) / fast_end;
            envelope = sustain_level + (1.0 - sustain_level) * std::exp(-fast_decay * progress);
        } else {
            double dt = t - attack - fast_end;
            envelope = sustain_level * std::exp(-slow_decay * dt);
        }
        double tail = 0.1;
        if (t > duration - tail) {
            double fade = (duration - t) / tail;
            envelope *= fade * fade;
        }

        // 三弦叠加
        double sample = 0.0;
        for (int s = 0; s < 3; ++s) {
            double string_sample = 0.0;
            for (int h = 1; h <= max_harmonics; ++h) {
                double inharmonicity = 1.0 + 0.00012 * h * h * (1.0 + 1.5 / pitch_factor);
                double harmonic_freq = freq * h * inharmonicity * detunes[s];

                // 柔和钢琴：更深的衰减曲线，更温暖的音色
                double warm = 1.0 / std::pow(h, 1.4);
                double softer = 1.0 / std::pow(h, 2.2);
                double amp = vel_factor * warm + (1.0 - vel_factor) * softer;

                double h_decay = std::exp(-0.4 * h * t);
                string_sample += amp * h_decay * std::sin(2.0 * M_PI * harmonic_freq * t);
            }
            sample += detune_amps[s] * string_sample;
        }

        sample += body_res_amp * std::sin(2.0 * M_PI * body_res_freq * t) * envelope;

        samples[i] += static_cast<float>(envelope * 0.16 * vel_curve * sample);
    }

    return samples;
}

// ========== 混响 ==========

std::vector<float> AudioPlayer::applyReverb(const std::vector<float>& dry,
                                              float mix, float room_size,
                                              int sample_rate) {
    if (dry.empty()) return dry;

    int n = static_cast<int>(dry.size());
    std::vector<float> wet(n, 0.0f);

    // 4 个梳状滤波器（延迟时间用素数避免金属音）
    const int comb_delays[] = {1687, 2053, 2501, 3001};
    const int num_combs = 4;
    double feedback = 0.28 + room_size * 0.42;  // 0.28~0.70

    std::vector<std::vector<float>> comb_buffers(num_combs);
    std::vector<int> comb_pos(num_combs, 0);
    for (int c = 0; c < num_combs; ++c) {
        comb_buffers[c].assign(comb_delays[c], 0.0f);
    }

    // 梳状滤波器阶段
    for (int i = 0; i < n; ++i) {
        float combined = 0.0f;
        for (int c = 0; c < num_combs; ++c) {
            float delayed = comb_buffers[c][comb_pos[c]];
            float input = dry[i] + delayed * feedback;
            comb_buffers[c][comb_pos[c]] = input;
            comb_pos[c] = (comb_pos[c] + 1) % comb_delays[c];
            // 简单低通滤波避免高频堆积
            comb_buffers[c][comb_pos[c]] = 0.5f * comb_buffers[c][comb_pos[c]]
                                          + 0.5f * input;
            combined += delayed;
        }
        wet[i] = combined / num_combs;
    }

    // 2 个全通滤波器
    const int ap_delays[] = {547, 719};
    const int num_aps = 2;
    double ap_feedback = 0.5;

    std::vector<std::vector<float>> ap_buffers(num_aps);
    std::vector<int> ap_pos(num_aps, 0);
    for (int a = 0; a < num_aps; ++a) {
        ap_buffers[a].assign(ap_delays[a], 0.0f);
    }

    for (int i = 0; i < n; ++i) {
        float sample = wet[i];
        for (int a = 0; a < num_aps; ++a) {
            float delayed = ap_buffers[a][ap_pos[a]];
            float input = sample + delayed * ap_feedback;
            ap_buffers[a][ap_pos[a]] = input;
            ap_pos[a] = (ap_pos[a] + 1) % ap_delays[a];
            sample = input - delayed * ap_feedback;
        }
        wet[i] = sample;
    }

    // 混合 dry/wet
    std::vector<float> output(n);
    float dry_mix = 1.0f - mix;
    for (int i = 0; i < n; ++i) {
        output[i] = dry_mix * dry[i] + mix * wet[i];
    }

    return output;
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

void AudioPlayer::playComposition(const std::vector<TimedNote>& notes,
                                   const std::string& mood,
                                   const std::string& save_path) {
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

    // 计算总时长
    double total_duration = 0.0;
    for (const auto& n : notes) {
        double end = n.start_time + n.duration;
        if (end > total_duration) total_duration = end;
    }

    int total_samples = static_cast<int>(total_duration * sample_rate) + sample_rate;
    std::vector<float> buffer(total_samples, 0.0f);

    // 逐个合成并叠加到时间线上
    for (const auto& note : notes) {
        auto wave = generateNote(note.pitch, note.duration, timbre,
                                 note.velocity, sample_rate);
        int start = static_cast<int>(note.start_time * sample_rate);
        for (int i = 0; i < static_cast<int>(wave.size())
                         && start + i < total_samples; ++i) {
            buffer[start + i] += wave[i];
        }
    }

    // 归一化防削波
    float max_val = 0.01f;
    for (auto s : buffer) {
        float a = std::fabs(s);
        if (a > max_val) max_val = a;
    }
    float norm_scale = 0.9f / max_val;
    for (auto& s : buffer) {
        s *= norm_scale;
    }

    // 施加混响（音乐厅级别）
    buffer = applyReverb(buffer, 0.30f, 0.8f, sample_rate);

    // 淡入淡出
    int fade_in = sample_rate / 20;    // 50ms
    int fade_out = sample_rate / 10;   // 100ms
    for (int i = 0; i < fade_in && i < total_samples; ++i) {
        buffer[i] *= static_cast<float>(i) / fade_in;
    }
    for (int i = 0; i < fade_out && i < total_samples; ++i) {
        int idx = total_samples - 1 - i;
        buffer[idx] *= static_cast<float>(i) / fade_out;
    }

    double play_duration = buffer.size() / static_cast<double>(sample_rate);
    std::cout << "[AudioPlayer] 混合合成 " << notes.size() << " 个音符, "
              << play_duration << " 秒, 音色: " << mood << std::endl;

    std::string wav_path = "/tmp/emotion_music.wav";
    if (!writeWavFile(wav_path, buffer, sample_rate)) {
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

    // 保存到指定路径（如果提供了 save_path）
    if (!save_path.empty()) {
        // 自动创建目录
        auto last_slash = save_path.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string dir = save_path.substr(0, last_slash);
            std::string mkdir_cmd = "mkdir -p " + dir;
            std::system(mkdir_cmd.c_str());
        }
        if (writeWavFile(save_path, buffer, sample_rate)) {
            std::cout << "[AudioPlayer] 已保存到: " << save_path << std::endl;
        } else {
            std::cerr << "[AudioPlayer] 保存失败: " << save_path << std::endl;
        }
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
