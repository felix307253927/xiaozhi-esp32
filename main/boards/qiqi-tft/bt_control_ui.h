/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-15 13:36:10
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-15 14:23:08
 */
#pragma once

#include "../../bluetooth/bt_music_player.h"
#include "../../display/guider_240_240.h"

namespace xiaozhi {

class BtControlUI {
public:
    BtControlUI(LcdGui240Display* display);

    // 初始化蓝牙控制UI
    void Initialize(const std::string& device_name);
    
    // 更新UI状态
    void UpdateUI();

    // 处理按钮事件
    void OnPlayPauseClick();
    void OnNextClick();
    void OnPreviousClick();
    void OnVolumeUpClick();
    void OnVolumeDownClick();

    // 获取音乐播放器实例
    BtMusicPlayer& GetMusicPlayer() { return BtMusicPlayer::GetInstance(); }

private:
    void CreateUI();
    void UpdatePlayState();
    void UpdateConnectionState();
    void UpdateVolumeState();

    LcdGui240Display* display_;

    // UI elements
    lv_obj_t* container_;
    lv_obj_t* status_label_;
    lv_obj_t* play_btn_;
    lv_obj_t* next_btn_;
    lv_obj_t* prev_btn_;
    lv_obj_t* volume_label_;
};

} // namespace xiaozhi