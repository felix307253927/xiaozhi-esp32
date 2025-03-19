/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-18 21:59:04
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-19 12:34:49
 */
/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/touch_pad.h"
#include "tp_interrupt.h"

namespace TouchPad
{
  static const char *TAG = "Touch pad";

  // 初始化静态成员变量
  TouchEventCallback TouchController::callback_ = nullptr;
  QueueHandle_t TouchController::queue_ = nullptr;
  bool TouchController::initialized_ = false;

  // 定义常量
  constexpr int TOUCH_BUTTON_NUM = 4;
  constexpr int TOUCH_BUTTON_WATERPROOF_ENABLE = 1;
  constexpr int TOUCH_BUTTON_DENOISE_ENABLE = 1;
  constexpr int TOUCH_CHANGE_CONFIG = 0;

  static const touch_pad_t button[TOUCH_BUTTON_NUM] = {
      TOUCH_PAD_NUM9,
      TOUCH_PAD_NUM10,
      TOUCH_PAD_NUM11,
      TOUCH_PAD_NUM13,
      // If this pad be touched, other pads no response.
  };

  /*
   * Touch threshold. The threshold determines the sensitivity of the touch.
   * This threshold is derived by testing changes in readings from different touch channels.
   * If (raw_data - benchmark) > benchmark * threshold, the pad be activated.
   * If (raw_data - benchmark) < benchmark * threshold, the pad be inactivated.
   */
  static const float button_threshold[TOUCH_BUTTON_NUM] = {
      0.2, // 20%.
      0.2, // 20%.
      0.2, // 20%.
      0.1, // 10%.
  };

  // 单例获取方法
  TouchController &TouchController::GetInstance()
  {
    static TouchController instance;
    return instance;
  }

  // 获取队列句柄
  QueueHandle_t TouchController::GetQueueHandle() const
  {
    return queue_;
  }

  // 注册回调
  void TouchController::RegisterCallback(TouchEventCallback callback)
  {
    callback_ = callback;
  }

  // 中断回调函数
  void TouchController::TouchInterruptCallback(void *arg)
  {
    int task_awoken = pdFALSE;
    TouchEvent evt;

    evt.intr_mask = touch_pad_read_intr_status_mask();
    evt.pad_status = touch_pad_get_status();
    evt.pad_num = touch_pad_get_current_meas_channel();

    xQueueSendFromISR(queue_, &evt, &task_awoken);
    if (task_awoken == pdTRUE)
    {
      portYIELD_FROM_ISR();
    }
  }

  static void tp_example_set_thresholds()
  {
    uint32_t touch_value;
    for (int i = 0; i < TOUCH_BUTTON_NUM; i++)
    {
      // read benchmark value
      touch_pad_read_benchmark(button[i], &touch_value);
      // set interrupt threshold.
      touch_pad_set_thresh(button[i], touch_value * button_threshold[i]);
      ESP_LOGI(TAG, "touch pad [%d] base %" PRIu32 ", thresh %" PRIu32,
               button[i], touch_value, (uint32_t)(touch_value * button_threshold[i]));
    }
  }

  static void touchsensor_filter_set(touch_filter_mode_t mode)
  {
    /* Filter function */
    touch_filter_config_t filter_info = {
        .mode = mode,      // Test jitter and filter 1/4.
        .debounce_cnt = 1, // 1 time count.
        .noise_thr = 0,    // 50%
        .jitter_step = 4,  // use for jitter mode.
        .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
    };
    touch_pad_filter_set_config(&filter_info);
    touch_pad_filter_enable();
    ESP_LOGI(TAG, "touch pad filter init");
  }

  void TouchController::TouchEventTask(void *pvParameter)
  {
    TouchEvent evt = {0};
    static uint8_t guard_mode_flag = 0;

    /* Wait touch sensor init done */
    vTaskDelay(50 / portTICK_PERIOD_MS);
    tp_example_set_thresholds();

    while (1)
    {
      int ret = xQueueReceive(queue_, &evt, (TickType_t)portMAX_DELAY);
      if (ret != pdTRUE)
      {
        continue;
      }

      // 输出调试信息
      if (evt.intr_mask & TOUCH_PAD_INTR_MASK_ACTIVE)
      {
        ESP_LOGI(TAG, "TouchSensor [%" PRIu32 "] be activated, status mask 0x%" PRIu32 "", evt.pad_num, evt.pad_status);
      }
      if (evt.intr_mask & TOUCH_PAD_INTR_MASK_INACTIVE)
      {
        ESP_LOGI(TAG, "TouchSensor [%" PRIu32 "] be inactivated, status mask 0x%" PRIu32, evt.pad_num, evt.pad_status);
      }
      if (evt.intr_mask & TOUCH_PAD_INTR_MASK_SCAN_DONE)
      {
        ESP_LOGI(TAG, "The touch sensor group measurement is done [%" PRIu32 "].", evt.pad_num);
      }
      if (evt.intr_mask & TOUCH_PAD_INTR_MASK_TIMEOUT)
      {
        /* Add your exception handling in here. */
        ESP_LOGI(TAG, "Touch sensor channel %" PRIu32 " measure timeout. Skip this exception channel!!", evt.pad_num);
        touch_pad_timeout_resume(); // Point on the next channel to measure.
      }

      // 调用用户注册的回调函数处理事件
      if (callback_)
      {
        callback_(evt);
      }
    }
  }

  void TouchController::Init()
  {
    if (initialized_)
    {
      ESP_LOGI(TAG, "Touch controller already initialized");
      return;
    }

    if (queue_ == nullptr)
    {
      queue_ = xQueueCreate(TOUCH_BUTTON_NUM, sizeof(TouchEvent));
    }

    // Initialize touch pad peripheral, it will start a timer to run a filter
    ESP_LOGI(TAG, "Initializing touch pad");
    /* Initialize touch pad peripheral. */
    touch_pad_init();
    for (int i = 0; i < TOUCH_BUTTON_NUM; i++)
    {
      touch_pad_config(button[i]);
    }

#if TOUCH_CHANGE_CONFIG
    /* If you want change the touch sensor default setting, please write here(after initialize). There are examples: */
    touch_pad_set_measurement_interval(TOUCH_PAD_SLEEP_CYCLE_DEFAULT);
    touch_pad_set_charge_discharge_times(TOUCH_PAD_MEASURE_CYCLE_DEFAULT);
    touch_pad_set_voltage(TOUCH_PAD_HIGH_VOLTAGE_THRESHOLD, TOUCH_PAD_LOW_VOLTAGE_THRESHOLD, TOUCH_PAD_ATTEN_VOLTAGE_THRESHOLD);
    touch_pad_set_idle_channel_connect(TOUCH_PAD_IDLE_CH_CONNECT_DEFAULT);
    for (int i = 0; i < TOUCH_BUTTON_NUM; i++)
    {
      touch_pad_set_cnt_mode(button[i], TOUCH_PAD_SLOPE_DEFAULT, TOUCH_PAD_TIE_OPT_DEFAULT);
    }
#endif

#if TOUCH_BUTTON_DENOISE_ENABLE
    /* Denoise setting at TouchSensor 0. */
    touch_pad_denoise_t denoise = {
        /* The bits to be cancelled are determined according to the noise level. */
        .grade = TOUCH_PAD_DENOISE_BIT4,
        /* By adjusting the parameters, the reading of T0 should be approximated to the reading of the measured channel. */
        .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
    ESP_LOGI(TAG, "Denoise function init");
#endif

#if TOUCH_BUTTON_WATERPROOF_ENABLE
    /* Waterproof function */
    touch_pad_waterproof_t waterproof = {
        .guard_ring_pad = button[3], // If no ring pad, set 0;
        /* It depends on the number of the parasitic capacitance of the shield pad.
           Based on the touch readings of T14 and T0, estimate the size of the parasitic capacitance on T14
           and set the parameters of the appropriate hardware. */
        .shield_driver = TOUCH_PAD_SHIELD_DRV_L2,
    };
    touch_pad_waterproof_set_config(&waterproof);
    touch_pad_waterproof_enable();
    ESP_LOGI(TAG, "touch pad waterproof init");
#endif

    /* Filter setting */
    touchsensor_filter_set(TOUCH_PAD_FILTER_IIR_16);
    touch_pad_timeout_set(true, TOUCH_PAD_THRESHOLD_MAX);
    /* Register touch interrupt ISR, enable intr type. */
    touch_pad_isr_register(TouchInterruptCallback, nullptr, TOUCH_PAD_INTR_MASK_ALL);
    /* If you have other touch algorithm, you can get the measured value after the `TOUCH_PAD_INTR_MASK_SCAN_DONE` interrupt is generated. */
    touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE | TOUCH_PAD_INTR_MASK_TIMEOUT);

    /* Enable touch sensor clock. Work mode is "timer trigger". */
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();

    // Start a task to show what pads have been touched
    xTaskCreate(&TouchEventTask, "touch_pad_read_task", 2048, nullptr, 5, nullptr);

    initialized_ = true;
  }

  // 为了向后兼容，提供全局初始化函数
  void init()
  {
    TouchController::GetInstance().Init();
  }

  // 为了向后兼容，提供全局获取队列句柄函数
  QueueHandle_t getQueueHandle()
  {
    return TouchController::GetInstance().GetQueueHandle();
  }

} // namespace TouchPad