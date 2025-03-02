/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-02-16 09:27:24
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-02 21:39:41
 */
#include "guider_240_240.h"
#include "gui_guider.h"
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
#include <lvgl.h>
#include "audio_codec.h"
#include "application.h"
#include "board.h"

#define TAG "LcdGui240Display"
#define LCD_LEDC_CH LEDC_CHANNEL_0

LV_FONT_DECLARE(font_awesome_30_4);

// 添加全局变量定义
lv_ui guider_ui;

LcdGui240Display *LcdGui240Display::instance_ = nullptr;

LcdGui240Display::LcdGui240Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   gpio_num_t backlight_pin, bool backlight_output_invert,
                                   int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                                   DisplayFonts fonts)
    : panel_io_(panel_io), panel_(panel), backlight_pin_(backlight_pin), backlight_output_invert_(backlight_output_invert),
      fonts_(fonts)
{
    instance_ = this;

    width_ = width;
    height_ = height;

    // 创建背光渐变定时器
    const esp_timer_create_args_t timer_args = {
        .callback = [](void *arg)
        {
            LcdGui240Display *display = static_cast<LcdGui240Display *>(arg);
            display->OnBacklightTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "backlight_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &backlight_timer_));

    esp_timer_create_args_t notification_timer_args = {
        .callback = [](void *arg)
        {
            LcdGui240Display *display = static_cast<LcdGui240Display *>(arg);
            DisplayLockGuard lock(display);
            if (guider_ui.home_notification_label_ != nullptr)
            {
                lv_obj_add_flag(guider_ui.home_notification_label_, LV_OBJ_FLAG_HIDDEN);
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "notification_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&notification_timer_args, &notification_timer_));

    InitializeBacklight(backlight_pin);

    // draw white
    std::vector<uint16_t> buffer(width_, 0x0000);
    for (int y = 0; y < height_; y++)
    {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 4;
    port_cfg.task_affinity = 0;
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
    if (display_ == nullptr)
    {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0)
    {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // 初始化gui_guider UI
    setup_ui(&guider_ui);

    // 在设置UI之前初始化时间同步
    SetBacklight(brightness_);
    // 设置全局字体
    lv_obj_set_style_text_font(lv_screen_active(), fonts_.text_font, 0);
    // 设置全局字体颜色
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_white(), 0);
    // 设置全局背景颜色
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
}

LcdGui240Display::~LcdGui240Display()
{
    if (backlight_timer_ != nullptr)
    {
        esp_timer_stop(backlight_timer_);
        esp_timer_delete(backlight_timer_);
    }
    if (notification_timer_ != nullptr)
    {
        esp_timer_stop(notification_timer_);
        esp_timer_delete(notification_timer_);
    }

    // 不需要手动删除guider_ui中的对象，因为它们会随着屏幕删除而被清理

    if (display_ != nullptr)
    {
        lv_display_delete(display_);
    }

    if (panel_ != nullptr)
    {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr)
    {
        esp_lcd_panel_io_del(panel_io_);
    }

    instance_ = nullptr;
}

void LcdGui240Display::InitializeBacklight(gpio_num_t backlight_pin)
{
    if (backlight_pin == GPIO_NUM_NC)
    {
        return;
    }

    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t backlight_channel = {
        .gpio_num = backlight_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = backlight_output_invert_,
        }};
    const ledc_timer_config_t backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 20000, // 背光pwm频率需要高一点，防止电感啸叫
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false};

    ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));
}

void LcdGui240Display::OnBacklightTimer()
{
    if (current_brightness_ < brightness_)
    {
        current_brightness_++;
    }
    else if (current_brightness_ > brightness_)
    {
        current_brightness_--;
    }

    // LEDC resolution set to 10bits, thus: 100% = 1023
    uint32_t duty_cycle = (1023 * current_brightness_) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH);

    if (current_brightness_ == brightness_)
    {
        esp_timer_stop(backlight_timer_);
    }
}

void LcdGui240Display::SetBacklight(uint8_t brightness)
{
    if (backlight_pin_ == GPIO_NUM_NC)
    {
        return;
    }

    if (brightness > 100)
    {
        brightness = 100;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness);
    // 停止现有的定时器（如果正在运行）
    esp_timer_stop(backlight_timer_);

    Display::SetBacklight(brightness);
    // 启动定时器，每 5ms 更新一次
    ESP_ERROR_CHECK(esp_timer_start_periodic(backlight_timer_, 5 * 1000));
}

void LcdGui240Display::Update()
{
    auto &board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    {
        DisplayLockGuard lock(this);
        if (mute_label_ != nullptr)
        {
            // 如果静音状态改变，则更新图标
            if (codec->output_volume() == 0 && !muted_)
            {
                muted_ = true;
                lv_label_set_text(mute_label_, FONT_AWESOME_VOLUME_MUTE);
            }
            else if (codec->output_volume() > 0 && muted_)
            {
                muted_ = false;
                lv_label_set_text(mute_label_, "");
            }
        }
    }

    // 更新电池图标
    int battery_level;
    bool charging;
    const char *icon = nullptr;
    if (board.GetBatteryLevel(battery_level, charging))
    {
        if (charging)
        {
            icon = FONT_AWESOME_BATTERY_CHARGING;
        }
        else
        {
            const char *levels[] = {
                FONT_AWESOME_BATTERY_EMPTY, // 0-19%
                FONT_AWESOME_BATTERY_1,     // 20-39%
                FONT_AWESOME_BATTERY_2,     // 40-59%
                FONT_AWESOME_BATTERY_3,     // 60-79%
                FONT_AWESOME_BATTERY_FULL,  // 80-99%
                FONT_AWESOME_BATTERY_FULL,  // 100%
            };
            icon = levels[battery_level / 20];
        }
        DisplayLockGuard lock(this);
        if (battery_label_ != nullptr && battery_icon_ != icon)
        {
            battery_icon_ = icon;
            lv_label_set_text(battery_label_, battery_icon_);
        }
    }

    // 升级固件时，不读取 4G 网络状态，避免占用 UART 资源
    auto device_state = Application::GetInstance().GetDeviceState();
    static const std::vector<DeviceState> allowed_states = {
        kDeviceStateIdle,
        kDeviceStateStarting,
        kDeviceStateWifiConfiguring,
        kDeviceStateListening,
    };

    if (std::find(allowed_states.begin(), allowed_states.end(), device_state) != allowed_states.end())
    {
        icon = board.GetNetworkStateIcon();
        if (guider_ui.home_network_label_ != nullptr && network_icon_ != icon)
        {
            DisplayLockGuard lock(this);
            network_icon_ = icon;
            lv_label_set_text(guider_ui.home_network_label_, network_icon_);
        }
    }
    UpdateTime();
}

void LcdGui240Display::UpdateTime()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char time_str[9];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);

    if (show_home_screen_count == 10)
    {
        ESP_LOGI(TAG, "Updating time: %s", time_str);
    }
    DisplayLockGuard lock(this);
    // 只在时间同步后更新模拟时钟的值
    home_analog_analog_clock_1_sec_value = (timeinfo.tm_sec + 60 - 15) % 60;
    home_analog_analog_clock_1_min_value = (timeinfo.tm_min + 60 - 15) % 60;
    home_analog_analog_clock_1_hour_value = (timeinfo.tm_hour % 12 + 12 - 3) % 12; // 转换为12小时制

    // 更新数字时间标签
    if (guider_ui.home_time_label_ != nullptr && !lv_obj_has_flag(guider_ui.home, LV_OBJ_FLAG_HIDDEN))
    {
        lv_label_set_text(guider_ui.home_time_label_, time_str);
    }
    show_home_screen_count++;
    // 每15秒切换analog
    if (show_home_screen_count > 15)
    {
        show_home_screen_count = 0;
        ShowClock();
    }
}

bool LcdGui240Display::Lock(int timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void LcdGui240Display::Unlock()
{
    lvgl_port_unlock();
}

void LcdGui240Display::SetStatus(const char *status)
{
    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "Set status: %s", status);
    ShowHome();
    if (guider_ui.home_status_label_ != nullptr)
    {
        lv_label_set_text(guider_ui.home_status_label_, status);
    }
}

void LcdGui240Display::SetChatMessage(const char *role, const char *content)
{
    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "Set chat message: %s", content);
    ShowHome();
    if (guider_ui.home_chat_message_label_ != nullptr)
    {
        lv_label_set_text(guider_ui.home_chat_message_label_, content);
    }
}

void LcdGui240Display::ShowNotification(const std::string &notification, int duration_ms)
{
    ShowNotification(notification.c_str(), duration_ms);
}

void LcdGui240Display::ShowNotification(const char *notification, int duration_ms)
{
    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "Show notification: %s", notification);
    // ShowHome(false); // 显示聊天界面但不启动自动切换定时器
    if (guider_ui.home_notification_label_ != nullptr)
    {
        // lv_label_set_text(guider_ui.home_notification_label_, notification);
        // 设置定时器在duration_ms后隐藏通知
        // esp_timer_stop(notification_timer_);
        // ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, duration_ms * 1000));
    }
}

void LcdGui240Display::SetEmotion(const char *emotion)
{
    struct Emotion
    {
        const char *icon;
        const char *text;
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
        {"🙄", "confused"}};

    // 查找匹配的表情
    std::string_view emotion_view(emotion);
    auto it = std::find_if(emotions.begin(), emotions.end(),
                           [&emotion_view](const Emotion &e)
                           { return e.text == emotion_view; });

    ESP_LOGI(TAG, "Set emotion: %s", emotion);
    DisplayLockGuard lock(this);
    ShowHome(false); // 显示聊天界面但不启动自动切换定时器

    lv_obj_set_style_text_font(guider_ui.home_emotion_label_, fonts_.emoji_font, 0);

    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    if (it != emotions.end())
    {
        lv_label_set_text(guider_ui.home_emotion_label_, it->icon);
    }
    else
    {
        lv_label_set_text(guider_ui.home_emotion_label_, "😶");
    }
}

void LcdGui240Display::SetIcon(const char *icon)
{
    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "Set icon: %s", icon);
    ShowHome(false); // 显示聊天界面但不启动自动切换定时器
    lv_obj_set_style_text_font(guider_ui.home_emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(guider_ui.home_emotion_label_, icon);
}

void LcdGui240Display::ShowClock()
{
    // DisplayLockGuard lock(this);

    // 隐藏聊天页面
    if (guider_ui.home != nullptr)
    {
        lv_obj_add_flag(guider_ui.home, LV_OBJ_FLAG_HIDDEN);
    }

    // 如果时钟页面已存在
    if (guider_ui.home_analog != nullptr)
    {
        bool need_screen_load = (lv_scr_act() != guider_ui.home_analog);
        bool is_hidden = lv_obj_has_flag(guider_ui.home_analog, LV_OBJ_FLAG_HIDDEN);

        // 只有在需要切换屏幕时才调用 lv_scr_load_anim
        if (need_screen_load)
        {
            lv_scr_load_anim(guider_ui.home_analog, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
        }
        // 如果是隐藏状态，显示出来
        if (is_hidden)
        {
            lv_obj_clear_flag(guider_ui.home_analog, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else
    {
        // 如果时钟页面不存在，创建它
        ESP_LOGI(TAG, "Creating clock screen");
        setup_scr_home_analog(&guider_ui);
        lv_scr_load_anim(guider_ui.home_analog, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    }
}

void LcdGui240Display::ShowHome(bool start_auto_clock_timer)
{
    // 重置自动切换时钟定时器
    show_home_screen_count = 0;
    DisplayLockGuard lock(this);

    // 隐藏时钟页面
    if (guider_ui.home_analog != nullptr && !lv_obj_has_flag(guider_ui.home_analog, LV_OBJ_FLAG_HIDDEN))
    {
        ESP_LOGI(TAG, "Hiding clock screen");
        lv_obj_add_flag(guider_ui.home_analog, LV_OBJ_FLAG_HIDDEN);
    }

    // 如果聊天页面已存在
    if (guider_ui.home != nullptr)
    {
        ESP_LOGI(TAG, "show home screen");
        bool need_screen_load = (lv_scr_act() != guider_ui.home);
        bool is_hidden = lv_obj_has_flag(guider_ui.home, LV_OBJ_FLAG_HIDDEN);

        // 只有在需要切换屏幕时才调用 lv_scr_load_anim
        if (need_screen_load)
        {
            ESP_LOGI(TAG, "load to home screen");
            lv_scr_load_anim(guider_ui.home, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
        }
        // 如果是隐藏状态，显示出来
        if (is_hidden)
        {
            ESP_LOGI(TAG, "show home screen");
            lv_obj_clear_flag(guider_ui.home, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else
    {
        // 如果聊天页面不存在，创建它
        ESP_LOGI(TAG, "Creating home screen");
        setup_scr_home(&guider_ui);
        lv_scr_load_anim(guider_ui.home, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    }
}

void LcdGui240Display::ShowHome()
{
    ShowHome(true); // 默认启动自动切换定时器
}