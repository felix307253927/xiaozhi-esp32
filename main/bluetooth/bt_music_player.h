/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-15 13:31:49
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-15 14:22:02
 */
#pragma once

#include <string>
#include "bt_a2dp_sink.h"
#include "bt_audio_codec.h"

namespace xiaozhi {

class BtMusicPlayer {
public:
    static BtMusicPlayer& GetInstance();

    bool Initialize(const std::string& device_name);
    void Deinitialize();

    // 播放控制
    void Play();
    void Pause();
    void Next();
    void Previous();
    void SetVolume(uint8_t volume);
    uint8_t GetVolume() const;

    // 状态查询
    bool IsConnected() const;
    bool IsPlaying() const;
    std::string GetConnectedDeviceName() const;

private:
    BtMusicPlayer() = default;
    ~BtMusicPlayer() = default;

    // 禁止拷贝
    BtMusicPlayer(const BtMusicPlayer&) = delete;
    BtMusicPlayer& operator=(const BtMusicPlayer&) = delete;

    bool initialized_ = false;
    BtA2dpSink& a2dp_sink_ = BtA2dpSink::GetInstance();
    BtAudioCodec& audio_codec_ = BtAudioCodec::GetInstance();
};

} // namespace xiaozhi