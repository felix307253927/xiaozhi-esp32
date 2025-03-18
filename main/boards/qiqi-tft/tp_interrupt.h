#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/touch_pad.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    touch_pad_intr_mask_t intr_mask;
    uint32_t pad_num;
    uint32_t pad_status;
    uint32_t pad_val;
} touch_event_t;

void init(void);
QueueHandle_t getQueueHandle(void);

#ifdef __cplusplus
}
#endif