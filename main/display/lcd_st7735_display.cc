/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-02-16 09:27:24
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-08 10:40:15
 */
#include "lcd_st7735_display.h"

#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/ledc.h>
#include <vector>
#include <esp_lvgl_port.h>
#include <esp_timer.h>
#include <esp_sntp.h>
#include <esp_netif.h>
#include <wifi_station.h>

#include "board.h"

#define TAG "LcdST7735Display"
#define LCD_LEDC_CH LEDC_CHANNEL_0

LV_FONT_DECLARE(font_awesome_30_4);

LcdST7735Display* LcdST7735Display::instance_ = nullptr;

LcdST7735Display::LcdST7735Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           gpio_num_t backlight_pin, bool backlight_output_invert,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : panel_io_(panel_io), panel_(panel), backlight_pin_(backlight_pin), backlight_output_invert_(backlight_output_invert),
      fonts_(fonts) {
    instance_ = this;

    width_ = width;
    height_ = height;

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // 在设置UI之前初始化时间同步
    InitTimeSync();

    SetupUI();
}

LcdST7735Display::~LcdST7735Display() {
    // 然后再清理 LVGL 对象
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }
    if (chat_container_ != nullptr) {
        lv_obj_del(chat_container_);
    }
    if (display_ != nullptr) {
        lv_display_delete(display_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }

    if (time_timer_ != nullptr) {
        esp_timer_stop(time_timer_);
        esp_timer_delete(time_timer_);
    }

    if (sync_timer_ != nullptr) {
        esp_timer_stop(sync_timer_);
        esp_timer_delete(sync_timer_);
    }

    if (status_timer_ != nullptr) {
        esp_timer_stop(status_timer_);
        esp_timer_delete(status_timer_);
    }

    instance_ = nullptr;
}

void LcdST7735Display::InitTimeSync() {
    // 创建时间更新定时器
    const esp_timer_create_args_t time_timer_args = {
        .callback = [](void* arg) {
            LcdST7735Display* display = static_cast<LcdST7735Display*>(arg);
            display->UpdateTime();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "time_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&time_timer_args, &time_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(time_timer_, 1000000)); // 每秒更新一次
    
    // 创建重试定时器
    const esp_timer_create_args_t sync_timer_args = {
        .callback = [](void* arg) {
            LcdST7735Display* display = static_cast<LcdST7735Display*>(arg);
            display->RetryTimeSync();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sync_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&sync_timer_args, &sync_timer_));
    
    // 延迟启动重试定时器，给系统一些时间初始化网络
    ESP_ERROR_CHECK(esp_timer_start_once(sync_timer_, 3000000)); // 3秒后开始第一次尝试
}

void LcdST7735Display::RetryTimeSync() {
    if (time_synced_) {
        return;
    }
    
    // 检查WiFi连接状态
    auto& wifi = WifiStation::GetInstance();
    if (wifi.IsConnected()) {
        ESP_LOGI(TAG, "WiFi connected, starting SNTP");
        
        // 配置 SNTP
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "pool.ntp.org");
        sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
        sntp_set_time_sync_notification_cb(OnTimeSync);
        
        ESP_LOGI(TAG, "Initializing SNTP");
        sntp_init();
        
        // 启动定期检查
        ESP_ERROR_CHECK(esp_timer_start_periodic(sync_timer_, 5000000));
    } else {
        ESP_LOGW(TAG, "WiFi not connected, will retry time sync later");
        // 3秒后重试
        ESP_ERROR_CHECK(esp_timer_start_once(sync_timer_, 3000000));
    }
}

void LcdST7735Display::OnTimeSync(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronized from NTP server!");
    auto display = LcdST7735Display::GetInstance();
    if (display) {
        display->time_synced_ = true;
        
        // 设置时区为中国时区 (UTC+8)
        if (setenv("TZ", "CST-8", 1) != 0) {
            ESP_LOGE(TAG, "Failed to set timezone");
        } else {
            tzset();
            ESP_LOGI(TAG, "Timezone set to CST-8");
            
            // 时间同步成功后立即显示时间
            display->OnStatusTimer();
        }
    }
}

void LcdST7735Display::UpdateTime() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char time_str[9];
    if (!time_synced_) {
        strcpy(time_str, "--:--:--");
    } else {
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec);
    }
             
    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "Updating time: %s", time_str);
    if (time_label_ != nullptr) {
        lv_label_set_text(time_label_, time_str);
    }
}

bool LcdST7735Display::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void LcdST7735Display::Unlock() {
    lvgl_port_unlock();
}

void LcdST7735Display::SetupUI() {
    DisplayLockGuard lock(this);
    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 2, 0);
    lv_obj_set_style_pad_right(status_bar_, 2, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_flex_align(status_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // lv_obj_set_style_border_side(status_bar_, LV_BORDER_SIDE_BOTTOM, 0);
    
    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_pad_column(content_, 0, 0);
    lv_obj_set_size(content_, LV_HOR_RES, LV_VER_RES - fonts_.text_font->line_height);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    emotion_container_ = lv_obj_create(content_);
    lv_obj_set_size(emotion_container_, LV_HOR_RES, 34);
    lv_obj_set_style_border_width(emotion_container_, 0, 0);
    lv_obj_set_flex_flow(emotion_container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(emotion_container_, 0, 0);
    lv_obj_set_style_margin_all(emotion_container_, 0, 0);
    lv_obj_set_flex_align(emotion_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    emotion_label_ = lv_label_create(emotion_container_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);

    chat_container_ = lv_obj_create(content_);
    lv_obj_set_size(chat_container_, LV_HOR_RES, LV_VER_RES - 34 - fonts_.text_font->line_height); // 设置容器大小
    lv_obj_set_style_pad_all(chat_container_, 0, 0);
    lv_obj_set_style_pad_column(chat_container_, 0, 0);
    lv_obj_set_style_border_width(chat_container_, 0, 0);
    lv_obj_set_scrollbar_mode(chat_container_, LV_SCROLLBAR_MODE_AUTO); // 启用滚动条
    lv_obj_set_scroll_dir(chat_container_, LV_DIR_VER);
    lv_obj_set_style_margin_all(chat_container_, 0, 0);
    lv_obj_set_style_margin_top(chat_container_, -6, 0);

    chat_message_label_ = lv_label_create(chat_container_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // 设置为自动换行模式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐

    /* Status bar */
    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);  // 状态标签占用剩余空间
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(status_label_, "正在初始化");
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    time_label_ = lv_label_create(status_bar_);
    lv_label_set_text(time_label_, "00:00:00");
    lv_obj_set_style_text_align(time_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_flex_grow(time_label_, 1);  // 时间标签也占用剩余空间
    lv_obj_add_flag(time_label_, LV_OBJ_FLAG_HIDDEN);  // 初始时隐藏时间标签

    // notification_label_ 应该在最上层
    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "通知");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, FONT_AWESOME_BATTERY_CHARGING);
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    // 创建状态显示定时器
    const esp_timer_create_args_t status_timer_args = {
        .callback = [](void* arg) {
            LcdST7735Display* display = static_cast<LcdST7735Display*>(arg);
            display->OnStatusTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "status_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&status_timer_args, &status_timer_));
}

void LcdST7735Display::OnStatusTimer() {
    DisplayLockGuard lock(this);
    if (status_label_ != nullptr) {
        lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);  // 隐藏状态标签
    }
    if (time_label_ != nullptr && time_synced_) {
        lv_obj_clear_flag(time_label_, LV_OBJ_FLAG_HIDDEN);  // 显示时间标签
    }
}

void LcdST7735Display::SetStatus(const char* status) {
    DisplayLockGuard lock(this);
    
    // 停止之前的定时器（如果在运行）
    if (status_timer_ != nullptr) {
        esp_timer_stop(status_timer_);
    }
    
    // 隐藏时间标签
    if (time_label_ != nullptr) {
        lv_obj_add_flag(time_label_, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 更新状态文本
    if (status == nullptr || status[0] == '\0') {
        if (status_label_ != nullptr) {
            lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        }
        // 如果状态为空，立即显示时间
        OnStatusTimer();
    } else {
        if (status_label_ != nullptr) {
            lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(status_label_, status);
            
            // 5秒后自动切换回时间显示
            ESP_ERROR_CHECK(esp_timer_start_once(status_timer_, 5000000));  // 5秒 = 5000000微秒
        }
    }
}

void LcdST7735Display::SetEmotion(const char* emotion) {
    struct Emotion {
        const char* icon;
        const char* text;
    };

    static const std::vector<Emotion> emotions = {
        {"😶", "neutral"},
        {"🙂", "happy"},
        {"😆", "laughing"},
        {"😂", "funny"},
        {"😔", "sad"},
        {"😠", "angry"},
        {"😭", "crying"},
        {"😍", "loving"},
        {"😳", "embarrassed"},
        {"😯", "surprised"},
        {"😱", "shocked"},
        {"🤔", "thinking"},
        {"😉", "winking"},
        {"😎", "cool"},
        {"😌", "relaxed"},
        {"🤤", "delicious"},
        {"😘", "kissy"},
        {"😏", "confident"},
        {"😴", "sleepy"},
        {"😜", "silly"},
        {"🙄", "confused"}
    };
    
    // 查找匹配的表情
    std::string_view emotion_view(emotion);
    auto it = std::find_if(emotions.begin(), emotions.end(),
        [&emotion_view](const Emotion& e) { return e.text == emotion_view; });

    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }

    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    lv_obj_set_style_text_font(emotion_label_, fonts_.emoji_font, 0);
    if (it != emotions.end()) {
        lv_label_set_text(emotion_label_, it->icon);
    } else {
        lv_label_set_text(emotion_label_, "😶");
    }
}

void LcdST7735Display::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, icon);
}
