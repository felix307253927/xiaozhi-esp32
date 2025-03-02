/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-02-16 09:28:09
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-02 20:45:00
 */
#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"
#include "gui_guider.h"
#include "custom.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include <font_emoji.h>
#include <esp_sntp.h>

#include <atomic>

// 在类定义之前添加这些外部变量声明
#ifdef __cplusplus
extern "C" {
#endif

extern int home_analog_analog_clock_1_hour_value;
extern int home_analog_analog_clock_1_min_value;
extern int home_analog_analog_clock_1_sec_value;

#ifdef __cplusplus
}
#endif

class LcdGui240Display : public Display
{
private:
    static LcdGui240Display *instance_; // 添加静态实例指针
    bool auto_clock_timer_running_ = false;

protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    gpio_num_t backlight_pin_ = GPIO_NUM_NC;
    bool backlight_output_invert_ = false;

    lv_draw_buf_t draw_buf_;

    DisplayFonts fonts_;

    esp_timer_handle_t backlight_timer_ = nullptr;
    uint8_t current_brightness_ = 0;
    lv_obj_t *time_label_ = nullptr;
    int show_home_screen_count = 0;

    void OnBacklightTimer();
    void InitializeBacklight(gpio_num_t backlight_pin);
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;
    virtual void Update() override;
    virtual void UpdateTime();
    void ShowHome(bool start_auto_clock_timer);  // 添加带参数的 ShowHome 函数

public:
    static LcdGui240Display *GetInstance() { return instance_; } // 添加获取实例的方法
    LcdGui240Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                     gpio_num_t backlight_pin, bool backlight_output_invert,
                     int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                     DisplayFonts fonts);
    ~LcdGui240Display();

    virtual void SetEmotion(const char *emotion) override;
    virtual void SetIcon(const char *icon) override;
    virtual void SetBacklight(uint8_t brightness) override;
    virtual void SetStatus(const char *status) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void ShowNotification(const std::string &notification, int duration_ms) override;
    virtual void ShowNotification(const char *notification, int duration_ms) override;
    void ShowClock();
    void ShowHome();
};

#endif // LCD_DISPLAY_H
