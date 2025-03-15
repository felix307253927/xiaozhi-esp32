/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-15 13:34:30
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-15 14:23:01
 */
#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/guider_240_240.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "bt_control_ui.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <memory>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>
#include <esp_timer.h>

#define TAG "qiqitft"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

class qiqitft : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdGui240Display* display_;
    std::unique_ptr<xiaozhi::BtControlUI> bt_control_;

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        
        display_ = new LcdGui240Display(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4,
                                        .icon_font = &font_awesome_16_4,
                                        .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(),
                                    });
    }

    void InitializeButtons() {
        auto codec = GetAudioCodec();
        codec->SetOutputVolume(100);

        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            if (bt_control_) {
                bt_control_->OnVolumeUpClick();
            }
        });

        volume_up_button_.OnLongPress([this]() {
            if (bt_control_) {
                bt_control_->GetMusicPlayer().SetVolume(127);
                bt_control_->UpdateUI();
                GetDisplay()->ShowNotification("最大音量");
            }
        });

        volume_down_button_.OnClick([this]() {
            if (bt_control_) {
                bt_control_->OnVolumeDownClick();
            }
        });

        volume_down_button_.OnLongPress([this]() {
            if (bt_control_) {
                bt_control_->GetMusicPlayer().SetVolume(0);
                bt_control_->UpdateUI();
                GetDisplay()->ShowNotification("已静音");
            }
        });
    }

    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Backlight"));
    }

public:
    qiqitft()
        : boot_button_(BOOT_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeIot();
        
        // 初始化蓝牙界面
        bt_control_ = std::make_unique<xiaozhi::BtControlUI>(display_);
        bt_control_->Initialize("ESP32_BT_Speaker");

        // 添加定时器更新蓝牙界面状态
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto* self = static_cast<qiqitft*>(arg);
                if (self->bt_control_) {
                    self->bt_control_->UpdateUI();
                }
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "bt_ui_update",
            .skip_unhandled_events = true
        };
        
        esp_timer_handle_t timer_handle;
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, 1000000)); // 每秒更新一次
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                             AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
                                             AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    ~qiqitft() {
        if (display_) {
            delete display_;
        }
    }
};

DECLARE_BOARD(qiqitft);
