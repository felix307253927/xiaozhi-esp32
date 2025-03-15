/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-15 13:36:30
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-15 14:23:28
 */
#include "bt_control_ui.h"
#include "esp_log.h"

static const char* TAG = "BtControlUI";

namespace xiaozhi {

BtControlUI::BtControlUI(LcdGui240Display* display)
    : display_(display),
      container_(nullptr),
      status_label_(nullptr),
      play_btn_(nullptr),
      next_btn_(nullptr),
      prev_btn_(nullptr),
      volume_label_(nullptr) {
}

void BtControlUI::Initialize(const std::string& device_name) {
    // 初始化蓝牙音乐播放器
    if (!GetMusicPlayer().Initialize(device_name)) {
        ESP_LOGE(TAG, "Failed to initialize music player");
        return;
    }

    CreateUI();
    UpdateUI();
}

void BtControlUI::CreateUI() {
    // 创建主容器
    container_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(container_, 240, 120);
    lv_obj_align(container_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_all(container_, 10, 0);
    
    // 状态标签
    status_label_ = lv_label_create(container_);
    lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 0);
    lv_label_set_text(status_label_, "未连接");
    
    // 控制按钮
    lv_obj_t* ctrl_container = lv_obj_create(container_);
    lv_obj_set_size(ctrl_container, 200, 50);
    lv_obj_align(ctrl_container, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_flex_flow(ctrl_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 上一曲按钮
    prev_btn_ = lv_btn_create(ctrl_container);
    lv_obj_set_size(prev_btn_, 40, 40);
    lv_obj_add_event_cb(prev_btn_, [](lv_event_t* e) {
        auto* ui = static_cast<BtControlUI*>(lv_event_get_user_data(e));
        if (ui) ui->OnPreviousClick();
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* prev_label = lv_label_create(prev_btn_);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_center(prev_label);
    
    // 播放/暂停按钮
    play_btn_ = lv_btn_create(ctrl_container);
    lv_obj_set_size(play_btn_, 40, 40);
    lv_obj_add_event_cb(play_btn_, [](lv_event_t* e) {
        auto* ui = static_cast<BtControlUI*>(lv_event_get_user_data(e));
        if (ui) ui->OnPlayPauseClick();
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* play_label = lv_label_create(play_btn_);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_center(play_label);
    
    // 下一曲按钮
    next_btn_ = lv_btn_create(ctrl_container);
    lv_obj_set_size(next_btn_, 40, 40);
    lv_obj_add_event_cb(next_btn_, [](lv_event_t* e) {
        auto* ui = static_cast<BtControlUI*>(lv_event_get_user_data(e));
        if (ui) ui->OnNextClick();
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* next_label = lv_label_create(next_btn_);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_center(next_label);
    
    // 音量标签
    volume_label_ = lv_label_create(container_);
    lv_obj_align(volume_label_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(volume_label_, "音量: 50%");
}

void BtControlUI::UpdateUI() {
    UpdateConnectionState();
    UpdatePlayState();
    UpdateVolumeState();
}

void BtControlUI::UpdateConnectionState() {
    if (!status_label_) return;
    
    if (GetMusicPlayer().IsConnected()) {
        std::string device_name = GetMusicPlayer().GetConnectedDeviceName();
        lv_label_set_text(status_label_, ("已连接: " + device_name).c_str());
    } else {
        lv_label_set_text(status_label_, "等待连接...");
    }
}

void BtControlUI::UpdatePlayState() {
    if (!play_btn_) return;
    
    lv_obj_t* play_label = lv_obj_get_child(play_btn_, 0);
    if (play_label) {
        if (GetMusicPlayer().IsPlaying()) {
            lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
        } else {
            lv_label_set_text(play_label, LV_SYMBOL_PLAY);
        }
    }
}

void BtControlUI::UpdateVolumeState() {
    if (!volume_label_) return;
    
    uint8_t volume = GetMusicPlayer().GetVolume();
    char buf[32];
    snprintf(buf, sizeof(buf), "音量: %d%%", (volume * 100) / 127);
    lv_label_set_text(volume_label_, buf);
}

void BtControlUI::OnPlayPauseClick() {
    if (GetMusicPlayer().IsPlaying()) {
        GetMusicPlayer().Pause();
    } else {
        GetMusicPlayer().Play();
    }
    UpdatePlayState();
}

void BtControlUI::OnNextClick() {
    GetMusicPlayer().Next();
}

void BtControlUI::OnPreviousClick() {
    GetMusicPlayer().Previous();
}

void BtControlUI::OnVolumeUpClick() {
    uint8_t current_volume = GetMusicPlayer().GetVolume();
    if (current_volume < 127) {
        GetMusicPlayer().SetVolume(current_volume + 10);
        UpdateVolumeState();
    }
}

void BtControlUI::OnVolumeDownClick() {
    uint8_t current_volume = GetMusicPlayer().GetVolume();
    if (current_volume > 0) {
        GetMusicPlayer().SetVolume(current_volume - 10);
        UpdateVolumeState();
    }
}

} // namespace xiaozhi