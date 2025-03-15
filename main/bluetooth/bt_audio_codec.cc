/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-15 14:19:30
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-15 14:19:53
 */
#include "bt_audio_codec.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "BtAudioCodec";

namespace xiaozhi {

BtAudioCodec::BtAudioCodec() : AudioCodec() {
    output_channels_ = 2;  // A2DP通常是立体声
    output_sample_rate_ = 44100;  // 标准A2DP采样率
    ESP_LOGI(TAG, "BtAudioCodec initialized");
}

BtAudioCodec& BtAudioCodec::GetInstance() {
    static BtAudioCodec instance;
    return instance;
}

void BtAudioCodec::ProcessData(const uint8_t* data, uint32_t len) {
    if (!data || len == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // 将8位无符号数据转换为16位有符号数据
    size_t samples = len / sizeof(uint8_t);
    std::vector<int16_t> pcm_data(samples);

    for (size_t i = 0; i < samples; i++) {
        // 将8位无符号数据(0-255)转换为16位有符号数据(-32768到32767)
        pcm_data[i] = (static_cast<int16_t>(data[i]) - 128) << 8;

        // 只有在缓冲区未满的情况下才添加数据
        if (audio_buffer_.size() < MAX_BUFFER_SIZE) {
            audio_buffer_.push(pcm_data[i]);
        }
    }
}

void BtAudioCodec::SetVolume(uint8_t volume) {
    volume_ = volume;
    // 将蓝牙音量(0-127)转换为AudioCodec音量(0-100)
    int codec_volume = (volume * 100) / 127;
    SetOutputVolume(codec_volume);
}

uint8_t BtAudioCodec::GetVolume() const {
    return volume_;
}

int BtAudioCodec::Read(int16_t* dest, int samples) {
    // A2DP只处理音频输出，不需要实现读取
    return 0;
}

int BtAudioCodec::Write(const int16_t* data, int samples) {
    // 将音频数据写入音频缓冲区
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    for (int i = 0; i < samples; i++) {
        if (audio_buffer_.size() < MAX_BUFFER_SIZE) {
            audio_buffer_.push(data[i]);
        }
    }
    
    return samples;
}

} // namespace xiaozhi