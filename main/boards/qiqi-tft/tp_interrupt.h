/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-19 14:30:00
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-19 15:00:00
 */
/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/touch_pad.h"
#include <functional>

namespace TouchPad
{
    // 触摸事件结构体
    struct TouchEvent
    {
        touch_pad_intr_mask_t intr_mask;
        uint32_t pad_num;
        uint32_t pad_status;
        uint32_t pad_val;
    };

    // 触摸事件回调函数类型
    using TouchEventCallback = std::function<void(const TouchEvent &)>;

    class TouchController
    {
    public:
        // 单例模式获取实例
        static TouchController &GetInstance();

        // 初始化触摸功能
        void Init();

        // 注册触摸事件回调
        void RegisterCallback(TouchEventCallback callback);

        // 获取触摸事件队列句柄
        QueueHandle_t GetQueueHandle() const;

    private:
        // 私有构造函数，防止外部创建实例
        TouchController() = default;

        // 防止拷贝和赋值
        TouchController(const TouchController &) = delete;
        TouchController &operator=(const TouchController &) = delete;

        static TouchEventCallback callback_;
        static QueueHandle_t queue_;
        static bool initialized_;

        // 处理触摸事件的任务函数
        static void TouchEventTask(void *arg);

        // 触摸中断回调函数
        static void TouchInterruptCallback(void *arg);
    };

} // namespace TouchPad