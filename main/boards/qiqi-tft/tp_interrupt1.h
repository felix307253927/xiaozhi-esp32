/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-18 23:20:10
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-18 23:20:17
 */
#pragma once

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/touch_pad.h"

typedef struct touch_msg {
    touch_pad_intr_mask_t intr_mask;
    uint32_t pad_num;
    uint32_t pad_status;
    uint32_t pad_val;
} touch_event_t;

class TouchPad {
public:
    TouchPad();
    void init();
    void setCallback(std::function<void(touch_event_t)> callback) {
        touch_callback_ = callback;
    }

private:
    static void touchsensor_interrupt_cb(void *arg);
    static void tp_read_task(void *pvParameter);
    void tp_set_thresholds();
    void touchsensor_filter_set(touch_filter_mode_t mode);

    QueueHandle_t que_touch;
    touch_pad_t button[5];
    float button_threshold[5];
    std::function<void(touch_event_t)> touch_callback_;
    
    static TouchPad* instance_;
};