/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-02-14 19:48:34
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-23 22:56:37
 */
/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-02-14 19:48:34
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-23 22:35:45
 */
#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>

LV_FONT_DECLARE(lv_font_Time_96);
LV_IMAGE_DECLARE(_angry_0_RGB565A8_64x64);
LV_IMAGE_DECLARE(_confused_0_RGB565A8_64x64);
LV_IMAGE_DECLARE(_crying_0_RGB565A8_64x64);
LV_IMAGE_DECLARE(_listen_0_RGB565A8_64x64);
LV_IMAGE_DECLARE(_speak_0_RGB565A8_64x64);
class LcdDisplay : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    // 时钟控件
    lv_obj_t* clock_screen_ = nullptr;
    lv_obj_t* time_txt_ = nullptr;

    lv_obj_t* emo_box_ = nullptr;
    lv_obj_t* emo_img_ = nullptr;

    int32_t clock_time_count = 1;

    DisplayFonts fonts_;

    void SetupUI();
    void SetClockUI(lv_obj_t *parent);
    virtual void SetClockTime() override;
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

protected:
    // 添加protected构造函数
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
        : panel_io_(panel_io), panel_(panel), fonts_(fonts) {}
    
public:
    ~LcdDisplay();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
    void SetClockBg(const void *value);
    void SetChatBg(const void *value);
    virtual bool SetEmoImg(const char* value) override;
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    virtual void SetChatMessage(const char* role, const char* content) override; 
#endif  

    // Add theme switching function
    virtual void SetTheme(const std::string& theme_name) override;
};

// RGB LCD显示器
class RgbLcdDisplay : public LcdDisplay {
public:
    RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// MIPI LCD显示器
class MipiLcdDisplay : public LcdDisplay {
public:
    MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// // SPI LCD显示器
class SpiLcdDisplay : public LcdDisplay {
public:
    SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// QSPI LCD显示器
class QspiLcdDisplay : public LcdDisplay {
public:
    QspiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// MCU8080 LCD显示器
class Mcu8080LcdDisplay : public LcdDisplay {
public:
    Mcu8080LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy,
                      DisplayFonts fonts);
};

class ClockSpiLcdDisplay : public LcdDisplay {
public:
ClockSpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy,
                      DisplayFonts fonts);
};
#endif // LCD_DISPLAY_H
