/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-02-16 09:28:09
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-19 21:52:53
 */
#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include <font_emoji.h>

#include <atomic>

class LcdST7735Display : public Display {
private:
    static LcdST7735Display* instance_;  // 添加静态实例指针

protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    gpio_num_t backlight_pin_ = GPIO_NUM_NC;
    bool backlight_output_invert_ = false;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* emotion_container_ = nullptr;
    lv_obj_t* chat_container_ = nullptr;
    int show_home_screen_count = 0;

    DisplayFonts fonts_;

    lv_obj_t* time_label_ = nullptr;
    bool time_synced_ = false;

    esp_timer_handle_t status_timer_ = nullptr;  // 添加状态显示定时器
    void OnStatusTimer();  // 添加状态定时器回调

    virtual void SetupUI();
    virtual void UpdateTime();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

public:
    static LcdST7735Display* GetInstance() { return instance_; }  // 添加获取实例的方法
    LcdST7735Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height,  int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
    ~LcdST7735Display();

    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
    virtual void SetStatus(const char* status) override;
};

#endif // LCD_DISPLAY_H
