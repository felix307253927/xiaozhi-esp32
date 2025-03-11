#include "bt_music_player.h"
#include <esp_log.h>

#define TAG "BtMusicPlayer"

BtMusicPlayer::BtMusicPlayer() : audio_codec_(nullptr) {
}

BtMusicPlayer::~BtMusicPlayer() {
    Deinit();
}

bool BtMusicPlayer::Init(const std::string& device_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        ESP_LOGW(TAG, "BtMusicPlayer already initialized");
        return true;
    }
    
    // 创建蓝牙音频编解码器
    audio_codec_ = std::make_unique<BtAudioCodec>(44100, 2);
    if (!audio_codec_) {
        ESP_LOGE(TAG, "Failed to create BtAudioCodec");
        return false;
    }
    
    // 启动音频编解码器
    audio_codec_->Start();
    
    // 启用输出
    audio_codec_->EnableOutput(true);
    
    // 设置默认音量
    audio_codec_->SetOutputVolume(70);
    
    ESP_LOGI(TAG, "BtMusicPlayer initialized, device name: %s", device_name.c_str());
    initialized_ = true;
    return true;
}

void BtMusicPlayer::Deinit() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    // 禁用输出
    if (audio_codec_) {
        audio_codec_->EnableOutput(false);
    }
    
    // 销毁音频编解码器
    audio_codec_.reset();
    
    initialized_ = false;
    ESP_LOGI(TAG, "BtMusicPlayer deinitialized");
}

bool BtMusicPlayer::IsConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        return false;
    }
    
    return audio_codec_->IsConnected();
}

bool BtMusicPlayer::IsPlaying() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        return false;
    }
    
    return audio_codec_->IsPlaying();
}

std::string BtMusicPlayer::GetConnectedDeviceName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        return "";
    }
    
    return audio_codec_->GetConnectedDeviceName();
}

std::string BtMusicPlayer::GetConnectedDeviceAddress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        return "";
    }
    
    return audio_codec_->GetConnectedDeviceAddress();
}

bool BtMusicPlayer::Play() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        ESP_LOGW(TAG, "BtMusicPlayer not initialized");
        return false;
    }
    
    return audio_codec_->Play();
}

bool BtMusicPlayer::Pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        ESP_LOGW(TAG, "BtMusicPlayer not initialized");
        return false;
    }
    
    return audio_codec_->Pause();
}

bool BtMusicPlayer::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        ESP_LOGW(TAG, "BtMusicPlayer not initialized");
        return false;
    }
    
    return audio_codec_->Stop();
}

bool BtMusicPlayer::Next() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        ESP_LOGW(TAG, "BtMusicPlayer not initialized");
        return false;
    }
    
    return audio_codec_->Next();
}

bool BtMusicPlayer::Previous() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        ESP_LOGW(TAG, "BtMusicPlayer not initialized");
        return false;
    }
    
    return audio_codec_->Previous();
}

void BtMusicPlayer::SetVolume(int volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        ESP_LOGW(TAG, "BtMusicPlayer not initialized");
        return;
    }
    
    audio_codec_->SetOutputVolume(volume);
}

int BtMusicPlayer::GetVolume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !audio_codec_) {
        ESP_LOGW(TAG, "BtMusicPlayer not initialized");
        return 0;
    }
    
    return audio_codec_->output_volume();
} 