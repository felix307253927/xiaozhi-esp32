#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>

class GuiDisplay : public Display {
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
    lv_obj_t* hour_needle_ = nullptr;
    lv_obj_t* min_needle_ = nullptr;
    lv_obj_t* sec_needle_ = nullptr;
    lv_obj_t* time_txt_ = nullptr;
    int32_t hour_ = 0;
    int32_t min_ = 0; 
    int32_t sec_ = 0;
    int32_t hour_len_ = 90;
    int32_t min_len_ = 120;
    int32_t sec_len_ = 150;
    int32_t clock_time_count = 1;

    DisplayFonts fonts_;

    void SetupUI();
    void SetClockUI(lv_obj_t *parent);
    virtual void SetClockTime() override;
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

protected:
    // 添加protected构造函数
    GuiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
        : panel_io_(panel_io), panel_(panel), fonts_(fonts) {}
    
public:
    ~GuiDisplay();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
    void SetClockBg(const void *value);
    void SetChatBg(const void *value);
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    virtual void SetChatMessage(const char* role, const char* content) override; 
#endif  

    // Add theme switching function
    virtual void SetTheme(const std::string& theme_name) override;
};

// RGB LCD显示器
class RgbGuiDisplay : public GuiDisplay {
public:
    RgbGuiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// MIPI LCD显示器
class MipiGuiDisplay : public GuiDisplay {
public:
    MipiGuiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// // SPI LCD显示器
class SpiGuiDisplay : public GuiDisplay {
public:
    SpiGuiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// QSPI LCD显示器
class QspiGuiDisplay : public GuiDisplay {
public:
    QspiGuiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// MCU8080 LCD显示器
class Mcu8080GuiDisplay : public GuiDisplay {
public:
    Mcu8080GuiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy,
                      DisplayFonts fonts);
};
#endif // LCD_DISPLAY_H
