/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-15 13:32:15
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-15 14:24:39
 */
#include "bt_music_player.h"
#include "esp_log.h"

static const char* TAG = "BtMusicPlayer";

namespace xiaozhi {

BtMusicPlayer& BtMusicPlayer::GetInstance() {
    static BtMusicPlayer instance;
    return instance;
}

bool BtMusicPlayer::Initialize(const std::string& device_name) {
    if (initialized_) {
        return true;
    }

    // 初始化A2DP接收器
    if (!a2dp_sink_.Init(device_name)) {
        ESP_LOGE(TAG, "Failed to initialize A2DP sink");
        return false;
    }

    // 音频编解码器是自我初始化的单例
    audio_codec_.Start();

    initialized_ = true;
    ESP_LOGI(TAG, "Bluetooth music player initialized");
    return true;
}

void BtMusicPlayer::Deinitialize() {
    if (!initialized_) {
        return;
    }

    // 只需停止A2DP，BtAudioCodec作为单例会自行管理
    a2dp_sink_.Deinit();
    initialized_ = false;
    ESP_LOGI(TAG, "Bluetooth music player deinitialized");
}

void BtMusicPlayer::Play() {
    if (!initialized_) {
        return;
    }
    a2dp_sink_.PlayControl(ESP_AVRC_PT_CMD_PLAY);
}

void BtMusicPlayer::Pause() {
    if (!initialized_) {
        return;
    }
    a2dp_sink_.PlayControl(ESP_AVRC_PT_CMD_PAUSE);
}

void BtMusicPlayer::Next() {
    if (!initialized_) {
        return;
    }
    a2dp_sink_.PlayControl(ESP_AVRC_PT_CMD_FORWARD);
}

void BtMusicPlayer::Previous() {
    if (!initialized_) {
        return;
    }
    a2dp_sink_.PlayControl(ESP_AVRC_PT_CMD_BACKWARD);
}

void BtMusicPlayer::SetVolume(uint8_t volume) {
    if (!initialized_) {
        return;
    }
    audio_codec_.SetVolume(volume);
    a2dp_sink_.SetVolume(volume);
}

uint8_t BtMusicPlayer::GetVolume() const {
    return audio_codec_.GetVolume();
}

bool BtMusicPlayer::IsConnected() const {
    return a2dp_sink_.IsConnected();
}

bool BtMusicPlayer::IsPlaying() const {
    return a2dp_sink_.IsPlaying();
}

std::string BtMusicPlayer::GetConnectedDeviceName() const {
    return a2dp_sink_.GetConnectedDeviceName();
}

} // namespace xiaozhi