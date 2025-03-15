#pragma once

#include <functional>
#include <memory>
#include <string>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "../audio_processing/audio_processor.h"

namespace xiaozhi {

class BluetoothManager {
public:
    static BluetoothManager* GetInstance();
    
    bool Initialize();
    void Deinitialize();
    
    // 设置设备名称
    void SetDeviceName(const std::string& name);
    
    // 获取连接状态
    bool IsConnected() const;
    
    // AVRCP控制接口
    bool PlayMusic();
    bool PauseMusic();
    bool NextTrack();
    bool PreviousTrack();
    bool SetVolume(uint8_t volume);
    uint8_t GetVolume() const;

    // 回调设置
    void SetConnectionCallback(std::function<void(bool)> callback);
    void SetAudioDataCallback(std::function<void(const uint8_t*, size_t)> callback);

private:
    BluetoothManager();
    ~BluetoothManager();
    
    // 禁止拷贝
    BluetoothManager(const BluetoothManager&) = delete;
    BluetoothManager& operator=(const BluetoothManager&) = delete;

    // ESP32回调处理函数
    static void HandleGapEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    static void HandleA2dpEvent(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param);
    static void HandleAvrcpEvent(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param);
    static void HandleA2dpData(const uint8_t* data, int len);

    static BluetoothManager* instance_;
    bool initialized_;
    bool connected_;
    uint8_t volume_;
    std::string device_name_;
    
    std::function<void(bool)> connection_callback_;
    std::function<void(const uint8_t*, size_t)> audio_data_callback_;
};

} // namespace xiaozhi