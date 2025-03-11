#include "bt_audio_codec.h"
#include <esp_log.h>
#include <string.h>

#define TAG "BtAudioCodec"
#define RING_BUFFER_SIZE (8 * 1024)

BtAudioCodec::BtAudioCodec(int output_sample_rate, int output_channels)
    : audio_ring_buffer_(nullptr) {
    // 设置音频参数
    output_sample_rate_ = output_sample_rate;
    output_channels_ = output_channels;
    input_sample_rate_ = 0;  // 不使用输入
    input_channels_ = 0;     // 不使用输入
    
    // 创建环形缓冲区
    audio_ring_buffer_ = xRingbufferCreate(RING_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (audio_ring_buffer_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create ring buffer");
    }
    
    // 设置设备名称
    device_name_ = "ESP32_BT_Speaker";
    
    // 初始化A2DP接收器
    BtA2dpSink::GetInstance().Init(device_name_);
    
    // 设置音频数据回调
    BtA2dpSink::GetInstance().SetAudioDataCallback(
        std::bind(&BtAudioCodec::OnA2dpAudioData, this, std::placeholders::_1, std::placeholders::_2));
    
    ESP_LOGI(TAG, "BtAudioCodec initialized, device name: %s", device_name_.c_str());
}

BtAudioCodec::~BtAudioCodec() {
    // 反初始化A2DP接收器
    BtA2dpSink::GetInstance().Deinit();
    
    // 销毁环形缓冲区
    if (audio_ring_buffer_ != nullptr) {
        vRingbufferDelete(audio_ring_buffer_);
        audio_ring_buffer_ = nullptr;
    }
    
    ESP_LOGI(TAG, "BtAudioCodec deinitialized");
}

void BtAudioCodec::SetOutputVolume(int volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 调用父类方法设置本地音量
    AudioCodec::SetOutputVolume(volume);
    
    // 设置蓝牙音量
    BtA2dpSink::GetInstance().SetVolume(volume);
}

void BtAudioCodec::EnableInput(bool enable) {
    // 蓝牙A2DP接收器不支持输入
    ESP_LOGW(TAG, "BtAudioCodec does not support input");
}

void BtAudioCodec::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    output_enabled_ = enable;
    
    if (enable) {
        ESP_LOGI(TAG, "BtAudioCodec output enabled");
    } else {
        ESP_LOGI(TAG, "BtAudioCodec output disabled");
    }
}

bool BtAudioCodec::IsConnected() const {
    return BtA2dpSink::GetInstance().IsConnected();
}

bool BtAudioCodec::IsPlaying() const {
    return BtA2dpSink::GetInstance().IsPlaying();
}

std::string BtAudioCodec::GetConnectedDeviceName() const {
    return BtA2dpSink::GetInstance().GetConnectedDeviceName();
}

std::string BtAudioCodec::GetConnectedDeviceAddress() const {
    return BtA2dpSink::GetInstance().GetConnectedDeviceAddress();
}

bool BtAudioCodec::Play() {
    return BtA2dpSink::GetInstance().PlayControl(ESP_AVRC_PT_CMD_PLAY);
}

bool BtAudioCodec::Pause() {
    return BtA2dpSink::GetInstance().PlayControl(ESP_AVRC_PT_CMD_PAUSE);
}

bool BtAudioCodec::Stop() {
    return BtA2dpSink::GetInstance().PlayControl(ESP_AVRC_PT_CMD_STOP);
}

bool BtAudioCodec::Next() {
    return BtA2dpSink::GetInstance().PlayControl(ESP_AVRC_PT_CMD_FORWARD);
}

bool BtAudioCodec::Previous() {
    return BtA2dpSink::GetInstance().PlayControl(ESP_AVRC_PT_CMD_BACKWARD);
}

int BtAudioCodec::Read(int16_t* dest, int samples) {
    // 蓝牙A2DP接收器不支持输入
    return 0;
}

int BtAudioCodec::Write(const int16_t* data, int samples) {
    // 蓝牙A2DP接收器不需要写入数据，因为数据是从蓝牙接收的
    return samples;
}

void BtAudioCodec::OnA2dpAudioData(const uint8_t* data, uint32_t len) {
    if (audio_ring_buffer_ == nullptr || !output_enabled_) {
        return;
    }
    
    // 将A2DP音频数据写入环形缓冲区
    BaseType_t ret = xRingbufferSend(audio_ring_buffer_, data, len, pdMS_TO_TICKS(10));
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "Failed to write audio data to ring buffer");
    }
    
    // 通知音频输出就绪
    if (on_output_ready_) {
        on_output_ready_();
    }
} 