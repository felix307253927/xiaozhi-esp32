根据 ESP-IDF 官方文档，我来详细介绍 ESP32S3 触摸传感器的灵敏度调整方法：

## TouchController 中的灵敏度设置

在当前代码中，灵敏度主要通过以下两个部分来控制：

1. **触摸阈值设置**：在`TouchController::SetThresholds()`函数中，通过`touch_pad_set_thresh()`函数设置了触摸阈值：

```cpp
void TouchController::SetThresholds()
{
  uint32_t touch_value;
  for (int i = 0; i < TOUCH_BUTTON_NUM; i++)
  {
    // 读取基准值
    touch_pad_read_benchmark(button_[i], &touch_value);
    // 设置中断阈值
    touch_pad_set_thresh(button_[i], touch_value * button_threshold_[i]);
    ESP_LOGI(TAG, "touch pad [%d] base %" PRIu32 ", thresh %" PRIu32,
             button_[i], touch_value, (uint32_t)(touch_value * button_threshold_[i]));
  }
}
```

2. **阈值百分比**：在类的静态常量数组中定义了每个触摸按钮的阈值百分比：

```cpp
const float TouchController::button_threshold_[TouchController::TOUCH_BUTTON_NUM] = {
    0.2, // 20%.
    0.2, // 20%.
    0.2, // 20%.
    0.1, // 10%.
};
```

## 触摸阈值工作原理

根据 ESP-IDF 文档，触摸传感器的工作原理是测量固定充放电次数所需的时钟周期数。当有手指触摸时，电容会发生变化，导致测量值(raw_data)与基准值(benchmark)之间产生差异。

触摸阈值的判定规则为：

- 如果 (raw_data - benchmark) > benchmark \* threshold，则触摸被激活
- 如果 (raw_data - benchmark) < benchmark \* threshold，则触摸被取消激活

## 调整触摸灵敏度的方法

要调整触摸传感器的灵敏度，可以从以下几个方面入手：

1. **调整阈值百分比**：

   - 降低`button_threshold_`数组中的值可以提高灵敏度
   - 增加`button_threshold_`数组中的值可以降低灵敏度
   - 当前代码中，TOUCH_PAD_NUM9、TOUCH_PAD_NUM10、TOUCH_PAD_NUM11 设置为 20%，TOUCH_PAD_NUM13 设置为 10%

2. **优化测量参数**：可以通过调用以下函数优化触摸测量参数：

   - `touch_pad_set_voltage()`：设置参考电压范围，缩小范围可以感知更细微的电容变化
   - `touch_pad_set_cnt_mode()`：设置斜率（速率）
   - `touch_pad_set_charge_discharge_times()`：设置单次测量的充放电次数

3. **滤波设置**：当前代码中使用了`FilterSet`函数设置了 IIR 滤波器：

   ```cpp
   FilterSet(TOUCH_PAD_FILTER_IIR_16);
   ```

   - 可以选择不同的滤波系数(IIR_4 到 IIR_256)或抖动滤波(JITTER)
   - 根据文档注意：ESP32S2 上 IIR 计算存在误差，建议使用较小的滤波系数

4. **降噪和防水功能**：
   - 代码中启用了降噪功能(TOUCH_BUTTON_DENOISE_ENABLE)
   - 也启用了防水功能(TOUCH_BUTTON_WATERPROOF_ENABLE)，使用 TOUCH_PAD_NUM13 作为防水保护通道

要提高触摸精度，还可以考虑以下调整：

1. 降低噪声干扰：

   ```cpp
   touch_pad_denoise_t denoise = {
       .grade = TOUCH_PAD_DENOISE_BIT4,     // 可以调整消除的比特数
       .cap_level = TOUCH_PAD_DENOISE_CAP_L4, // 可以调整内部参考电容
   };
   ```

2. 调整平滑级别：
   ```cpp
   filter_info.smh_lvl = TOUCH_PAD_SMOOTH_IIR_2; // 可以使用IIR_2/IIR_4/IIR_8
   ```

总之，要调整触摸灵敏度，主要的方法是修改`button_threshold_`数组中的阈值百分比，较小的值会提高灵敏度，较大的值会降低灵敏度。同时，可以结合优化测量参数、调整滤波设置和使用降噪/防水功能来获得最佳的触摸体验。
