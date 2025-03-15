/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-15 13:30:24
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-15 13:54:15
 */
#pragma once

#include <string>
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

namespace xiaozhi {

class BtA2dpSink {
public:
    static BtA2dpSink& GetInstance();

    // 初始化和反初始化
    bool Init(const std::string& device_name);
    void Deinit();

    // 播放控制
    void PlayControl(esp_avrc_pt_cmd_t cmd);
    void SetVolume(uint8_t volume);
    
    // 状态查询
    bool IsConnected() const;
    bool IsPlaying() const;
    std::string GetConnectedDeviceName() const;

private:
    BtA2dpSink() = default;
    ~BtA2dpSink() = default;
    
    // 禁止拷贝
    BtA2dpSink(const BtA2dpSink&) = delete;
    BtA2dpSink& operator=(const BtA2dpSink&) = delete;

    // ESP32回调处理函数
    static void GapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    static void A2dpCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param);
    static void AvrcpCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param);
    static void AudioDataCallback(const uint8_t* buf, uint32_t len);

    bool initialized_ = false;
    bool connected_ = false;
    bool playing_ = false;
    uint8_t volume_ = 0;
    std::string device_name_;
    std::string connected_device_name_;
};

} // namespace xiaozhi