# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "1.4.6")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 添加必要的ESP-IDF组件
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components)
set(COMPONENTS main bt esp_adc esp_peripherals nvs_flash driver esp_a2dp esp_avrc freertos esp_system esp_common esp_timer)

project(xiaozhi)
