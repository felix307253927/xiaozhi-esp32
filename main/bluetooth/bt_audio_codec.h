#ifndef _BT_AUDIO_CODEC_H_
#define _BT_AUDIO_CODEC_H_

#include "audio_codecs/audio_codec.h"
#include "bt_a2dp_sink.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>

#include <string>
#include <mutex>

class BtAudioCodec : public AudioCodec {
public:
    BtAudioCodec(int output_sample_rate = 44100, int output_channels = 2);
    virtual ~BtAudioCodec();
    
    // 重写AudioCodec的虚函数
    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;
    
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

protected:
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

private:
    // 处理A2DP音频数据的回调
    void OnA2dpAudioData(const uint8_t* data, uint32_t len);
    
    // 音频数据缓冲区
    RingbufHandle_t audio_ring_buffer_;
    std::mutex mutex_;
    std::string device_name_;
};

#endif // _BT_AUDIO_CODEC_H_ 