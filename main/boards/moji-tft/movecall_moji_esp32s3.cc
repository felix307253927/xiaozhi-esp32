#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "esp_system.h"
#include <strings.h>
#include <string.h>

#define TAG "MovecallMojiESP32S3"

LV_IMAGE_DECLARE(_nezha_RGB565A8_240x240);
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

static QueueHandle_t uart0_queue;
class MovecallMojiESP32S3 : public WifiBoard
{
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdDisplay *display_;

    void InitializeCodecI2c()
    {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    // SPI初始化
    void InitializeSpi()
    {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(DISPLAY_SPI_SCLK_PIN, DISPLAY_SPI_MOSI_PIN,
                                                              DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // GC9A01初始化
    void InitializeGc9a01Display()
    {
        ESP_LOGI(TAG, "Init GC9A01 display");

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(DISPLAY_SPI_CS_PIN, DISPLAY_SPI_DC_PIN, NULL, NULL);
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));

        ESP_LOGI(TAG, "Install GC9A01 panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN; // Set to -1 if not use
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;        // LCD_RGB_ENDIAN_RGB;
        panel_config.bits_per_pixel = 16;                    // Implemented by LCD command `3Ah` (16/18)

        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

        display_ = new ClockSpiLcdDisplay(io_handle, panel_handle,
                                          DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                          {
                                              .text_font = &font_puhui_20_4,
                                              .icon_font = &font_awesome_20_4,
                                              .emoji_font = font_emoji_64_init(),
                                          });
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            ESP_LOGI(TAG, "boot_button_ pressed");
            app.StartChatState(); });
        volume_up_button_.OnClick([this]()
                                  {
            ESP_LOGI(TAG, "volume_up_button_ pressed");
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); });
        volume_up_button_.OnLongPress([this]()
                                      {
            ESP_LOGI(TAG, "volume_up_button_ long_pressed");                      
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME); });

        volume_down_button_.OnClick([this]()
                                    {
            ESP_LOGI(TAG, "volume_down_button_ pressed");
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); });

        volume_down_button_.OnLongPress([this]()
                                        {
            ESP_LOGI(TAG, "volume_down_button_ long_pressed");
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED); });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
    }

    void InitializeUart()
    {
        // UART配置结构体
        const uart_config_t uart_config = {
            .baud_rate = 9600,                     // 波特率
            .data_bits = UART_DATA_8_BITS,         // 数据位
            .parity = UART_PARITY_DISABLE,         // 奇偶校验位
            .stop_bits = UART_STOP_BITS_1,         // 停止位
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // 流控制 这里禁用硬件控制
            .source_clk = UART_SCLK_DEFAULT,       // uart时钟源
        };
        uart_set_pin(UART_NUM_1, UART_TXD, UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        uart_driver_install(UART_NUM_1, BUF_SIZE * 1, BUF_SIZE * 1, 20, &uart0_queue, 0);
        uart_param_config(UART_NUM_1, &uart_config);

        xTaskCreate([](void *arg){
            MovecallMojiESP32S3 *board = (MovecallMojiESP32S3 *)arg;
            board->UART0_EVENT(NULL);
            vTaskDelete(NULL);
        }, "uart0_event", 4096, NULL, 12, NULL);
    }

    void UART0_EVENT(void *pvParameters)
    {

        uart_event_t event;                          // uart事件结构体
        size_t buffered_size;                        // 存放RX缓冲区里的数据长度   单位：字节
        uint8_t *dtmp = (uint8_t *)malloc(BUF_SIZE); // 开辟一段内存存放接收到的数据 无符号字节型（动态分配的内存dtmp的地址不可以发生改变）

        for (;;)
        {
            // 等得UART事件
            // 从uart事件队列中接收一个项目并拷贝到事件结构体中 成功接收到项目将从队列中删除
            if (xQueueReceive(uart0_queue, (void *)&event, (TickType_t)portMAX_DELAY))
            {
                // C语言函数 将指定长度的内存清零 这里如果有事件发生清零接收数据存放区
                bzero(dtmp, BUF_SIZE);

                switch (event.type)
                {
                case UART_FIFO_OVF: // RX FIFO缓存区溢出
                    ESP_LOGI(TAG, "[UART_FIFO_OVF]:The upper buffer is full causing the FIFO cache to overflow\n");
                    if (ESP_OK == uart_flush(UART_NUM_1)) // 这里直接清除FIFO

                        ESP_LOGI(TAG, "[UART_FIFO_OVF]:FIFO clearing is complete!\n");
                    else
                        ESP_LOGI(TAG, "[UART_FIFO_OVF]:Failed to clear FIFO败!!\n");
                    xQueueReset(uart0_queue); // 将队列初始化
                    break;
                case UART_DATA: // 接收到数据
                    // 这里需要注意!!!!
                    // ESP32默认的RX FIFO缓存区是128字节 当接收到120字节时就会触发此事件。如果需要接收大于120字节的数据，可以使用uart_event_t结构体中的timeout_flag成员来实现
                    // timeout_flag UART读取超时标志，官方翻译过来：
                    /*UART数据读取超时标志UART数据事件(在配置的RX输出期间没有接收到新数据)如果事件是由FIFO-full中断引起的，那么在下一个字节到来之前将没有带有超时标志的事件。
                 UART_DATA事件的UART数据读取超时标志(在配置的RX TOUT期间没有接收到新数据)如果该事件是由FIFO-full中断引起的，那么在下一个字节到来之前将没有带有超时标志的事件。*/

                    if (event.timeout_flag) // 发生超时。说明数据全部接收完成，可以开始处理了
                    {
                        uart_get_buffered_data_len(UART_NUM_1, &buffered_size); // 读取RX缓冲区里的数据长度
                        ESP_LOGI(TAG, "[UART_DATA] [%d] [%d]:", buffered_size, event.timeout_flag);
                        uart_read_bytes(UART_NUM_1, dtmp, buffered_size, portMAX_DELAY); // 这里的数据长度就不能按照例程里的使用event.size，因为这个值最大是120 也就是FIFO的最大接收长度
                        // 确保字符串以null结尾
                        dtmp[buffered_size] = '\0';
                        ESP_LOGI(TAG, "接收到的字符串: %s", (char *)dtmp);
                        if (strcmp((char *)dtmp, "open") == 0)
                        {
                            ESP_LOGI(TAG, "收到打开指令");
                        }
                        else if (strcmp((char *)dtmp, "+") == 0)
                        {
                            ESP_LOGI(TAG, "收到增加音量指令");
                            UpdateVolume(-1);
                        }
                        else if (strcmp((char *)dtmp, "-") == 0)
                        {
                            ESP_LOGI(TAG, "收到减少音量指令");
                             UpdateVolume(-2);
                        }
                        else
                        {
                            // 将接收到的字符串转换为数字
                            int number = atoi((char *)dtmp);
                            if (number > 0)
                            {
                                ESP_LOGI(TAG, "转换后的数字: %d", number);
                                UpdateVolume(number);
                            }
                        }
                        uart_write_bytes(UART_NUM_1, (const uint8_t *)dtmp, buffered_size);
                    }

                    break;
                case UART_BREAK:
                    ESP_LOGI(TAG, "[UART_event] [%d]:", event.size);

                    break;
                case UART_BUFFER_FULL:

                    break;
                case UART_FRAME_ERR:

                    break;
                case UART_PARITY_ERR:

                    break;
                case UART_DATA_BREAK:

                    break;
                case UART_PATTERN_DET:

                    break;

                default:
                    break;
                }
            }
        }
        free(dtmp);  // 释放内存
        dtmp = NULL; // 将指针地址指向NULL 防止误调用造成非法访问，
        vTaskDelete(NULL);
    }
    void UpdateVolume(int volume) {
        ESP_LOGI(TAG, "UpdateVolume");
            auto codec = GetAudioCodec();
            if (volume == -1){
                volume = codec->output_volume() + 10;
            } else if (volume == -2) {
                volume = codec->output_volume() - 10;
            }
            if (volume < 0) {
                volume = 0;
            } else if (volume > 100) {
                volume = 100;
            }
            ESP_LOGI(TAG, "Set Volume [%d]", volume);
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    }

public:
    MovecallMojiESP32S3() : boot_button_(BOOT_BUTTON_GPIO),
                            volume_up_button_(VOLUME_UP_BUTTON_GPIO),
                            volume_down_button_(VOLUME_DOWN_BUTTON_GPIO)
    {
        InitializeCodecI2c();
        InitializeSpi();
        InitializeGc9a01Display();
        InitializeButtons();
        InitializeIot();
        InitializeUart();
        GetBacklight()->RestoreBrightness();

        if (display_ != nullptr)
        {
            display_->SetClockBg(&_nezha_RGB565A8_240x240);
            // display_->SetChatBg(&chat_RGB565A8_360x360);
        }
    }

    virtual Led *GetLed() override
    {
        static SingleLed led_strip(BUILTIN_LED_GPIO);
        return &led_strip;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }

    virtual Backlight *GetBacklight() override
    {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                                            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }
};

DECLARE_BOARD(MovecallMojiESP32S3);
