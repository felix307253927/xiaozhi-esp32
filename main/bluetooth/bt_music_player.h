#ifndef _BT_MUSIC_PLAYER_H_
#define _BT_MUSIC_PLAYER_H_

#include "bt_audio_codec.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <string>
#include <memory>
#include <mutex>

class BtMusicPlayer {
public:
    static BtMusicPlayer& GetInstance() {
        static BtMusicPlayer instance;
        return instance;
    }
    
    // 删除拷贝构造函数和赋值运算符
    BtMusicPlayer(const BtMusicPlayer&) = delete;
    BtMusicPlayer& operator=(const BtMusicPlayer&) = delete;
    
    // 初始化蓝牙音乐播放器
    bool Init(const std::string& device_name = "ESP32_BT_Speaker");
    
    // 反初始化蓝牙音乐播放器
    void Deinit();
    
    // 获取蓝牙连接状态
    bool IsConnected() const;
    
    // 获取蓝牙播放状态
    bool IsPlaying() const;
    
    // 获取连接的设备名称
    std::string GetConnectedDeviceName() const;
    
    // 获取连接的设备地址
    std::string GetConnectedDeviceAddress() const;
    
    // 播放控制
    bool Play();
    bool Pause();
    bool Stop();
    bool Next();
    bool Previous();
    
    // 设置音量
    void SetVolume(int volume);
    
    // 获取音量
    int GetVolume() const;

private:
    BtMusicPlayer();
    ~BtMusicPlayer();
    
    std::unique_ptr<BtAudioCodec> audio_codec_;
    std::mutex mutex_;
    bool initialized_ = false;
};

#endif // _BT_MUSIC_PLAYER_H_ 