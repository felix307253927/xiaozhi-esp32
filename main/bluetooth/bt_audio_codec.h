/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-15 13:31:36
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-15 14:19:23
 */
#pragma once

#include <cstdint>
#include <vector>
#include <queue>
#include <mutex>
#include "../audio_codecs/audio_codec.h"
#include "esp_a2dp_api.h"

namespace xiaozhi {

// 蓝牙音频编解码器，继承自AudioCodec
class BtAudioCodec : public AudioCodec {
public:
    static BtAudioCodec& GetInstance();

    // 处理接收到的A2DP数据
    void ProcessData(const uint8_t* data, uint32_t len);

    // 设置音量
    void SetVolume(uint8_t volume);
    uint8_t GetVolume() const;

protected:
    // AudioCodec接口实现
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

private:
    BtAudioCodec();
    ~BtAudioCodec() = default;

    // 禁止拷贝
    BtAudioCodec(const BtAudioCodec&) = delete;
    BtAudioCodec& operator=(const BtAudioCodec&) = delete;

    bool initialized_ = false;
    uint8_t volume_ = 80;  // 默认音量

    // 音频缓冲区
    std::queue<int16_t> audio_buffer_;
    std::mutex buffer_mutex_;
    static constexpr size_t MAX_BUFFER_SIZE = 8192;  // 最大缓冲区大小
};

} // namespace xiaozhi