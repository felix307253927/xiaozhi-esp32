#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "esp_system.h"

#ifdef SH1106
#include <esp_lcd_panel_sh1106.h>
#endif
#include <strings.h>
#include <string.h>

#define TAG "CompactWifiBoard"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

#define UART_NUM UART_NUM_1
#define BUF_SIZE 2048

static QueueHandle_t uart0_queue;
class CompactWifiBoard : public WifiBoard
{
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display *display_ = nullptr;
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    void InitializeDisplayI2c()
    {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display()
    {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                   {&font_puhui_14_1, &font_awesome_14_1});
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });
        touch_button_.OnPressDown([this]()
                                  { Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]()
                                { Application::GetInstance().StopListening(); });

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
        thing_manager.AddThing(iot::CreateThing("Lamp"));
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

        xTaskCreate(UART0_EVENT, "uart0_event", 4096, NULL, 12, NULL);
    }

    static void UART0_EVENT(void *pvParameters)
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
                        }
                        else if (strcmp((char *)dtmp, "-") == 0)
                        {
                            ESP_LOGI(TAG, "收到减少音量指令");
                        }
                        else
                        {
                            // 将接收到的字符串转换为数字
                            int number = atoi((char *)dtmp);
                            if (number > 0)
                            {
                                ESP_LOGI(TAG, "转换后的数字: %d", number);
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

public:
    CompactWifiBoard() : boot_button_(BOOT_BUTTON_GPIO),
                         touch_button_(TOUCH_BUTTON_GPIO),
                         volume_up_button_(VOLUME_UP_BUTTON_GPIO),
                         volume_down_button_(VOLUME_DOWN_BUTTON_GPIO)
    {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeIot();
        InitializeUart();
    }

    virtual Led *GetLed() override
    {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }
};

DECLARE_BOARD(CompactWifiBoard);
