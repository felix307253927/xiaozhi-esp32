/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-11 21:34:15
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-11 21:35:42
 */
#ifndef _BT_A2DP_SINK_H_
#define _BT_A2DP_SINK_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>

#include <string>
#include <functional>
#include <vector>
#include <mutex>

class BtA2dpSink {
public:
    static BtA2dpSink& GetInstance() {
        static BtA2dpSink instance;
        return instance;
    }
    
    // 删除拷贝构造函数和赋值运算符
    BtA2dpSink(const BtA2dpSink&) = delete;
    BtA2dpSink& operator=(const BtA2dpSink&) = delete;

    // 初始化蓝牙A2DP接收器
    bool Init(const std::string& device_name);
    
    // 反初始化蓝牙A2DP接收器
    void Deinit();
    
    // 设置音频数据回调
    void SetAudioDataCallback(std::function<void(const uint8_t*, uint32_t)> callback);
    
    // 获取连接状态
    bool IsConnected() const { return connected_; }
    
    // 获取播放状态
    bool IsPlaying() const { return playing_; }
    
    // 获取连接的设备名称
    std::string GetConnectedDeviceName() const { return connected_device_name_; }
    
    // 获取连接的设备地址
    std::string GetConnectedDeviceAddress() const;
    
    // AVRCP控制命令
    bool PlayControl(esp_avrc_pt_cmd_t cmd);
    
    // 设置音量
    void SetVolume(uint8_t volume);
    
    // 获取音量
    uint8_t GetVolume() const { return volume_; }

private:
    BtA2dpSink();
    ~BtA2dpSink();

    // 蓝牙事件回调
    static void BtAppGapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    static void BtAppA2dCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param);
    static void BtAppAvrcCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param);
    static void BtAppA2dDataCallback(const uint8_t* data, uint32_t len);
    
    // 处理音频数据
    void ProcessAudioData(const uint8_t* data, uint32_t len);
    
    // 处理AVRCP元数据
    void ProcessAvrcMetadata(uint8_t attr_id, const uint8_t* attr_value, uint16_t attr_length);
    
    // 处理AVRCP通知
    void ProcessAvrcNotification(uint8_t event_id, esp_avrc_rn_param_t* event_parameter);

    bool initialized_ = false;
    bool connected_ = false;
    bool playing_ = false;
    uint8_t volume_ = 70;
    std::string device_name_;
    std::string connected_device_name_;
    esp_bd_addr_t peer_bda_ = {0};
    std::function<void(const uint8_t*, uint32_t)> audio_data_callback_ = nullptr;
    std::mutex mutex_;
};

#endif // _BT_A2DP_SINK_H_ 