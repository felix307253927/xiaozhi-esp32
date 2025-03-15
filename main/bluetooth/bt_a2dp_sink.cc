#include "bt_a2dp_sink.h"
#include "bt_audio_codec.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include <cstring>

using namespace xiaozhi;

static const char* TAG = "BtA2dpSink";

namespace xiaozhi {

BtA2dpSink& BtA2dpSink::GetInstance() {
    static BtA2dpSink instance;
    return instance;
}

bool BtA2dpSink::Init(const std::string& device_name) {
    if (initialized_) {
        return true;
    }

    ESP_LOGI(TAG, "Initializing Bluetooth A2DP Sink");
    device_name_ = device_name;

    // 初始化NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 释放经典蓝牙内存
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    // 初始化蓝牙控制器
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "initialize controller failed");
        return false;
    }

    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
        ESP_LOGE(TAG, "enable controller failed");
        return false;
    }

    if (esp_bluedroid_init() != ESP_OK) {
        ESP_LOGE(TAG, "initialize bluedroid failed");
        return false;
    }

    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(TAG, "enable bluedroid failed");
        return false;
    }

    // 设置设备名称
    esp_bt_dev_set_device_name(device_name_.c_str());

    // 设置可发现和可连接模式
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    // 注册GAP回调
    esp_bt_gap_register_callback(GapCallback);

    // 初始化A2DP SINK
    esp_a2d_register_callback(A2dpCallback);
    esp_a2d_sink_register_data_callback(AudioDataCallback);
    esp_a2d_sink_init();

    // 初始化AVRCP控制器
    esp_avrc_ct_init();
    esp_avrc_ct_register_callback(AvrcpCallback);

    initialized_ = true;
    ESP_LOGI(TAG, "Bluetooth A2DP Sink initialized");
    return true;
}

void BtA2dpSink::Deinit() {
    if (!initialized_) {
        return;
    }

    esp_avrc_ct_deinit();
    esp_a2d_sink_deinit();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    initialized_ = false;
    connected_ = false;
    playing_ = false;
}

void BtA2dpSink::PlayControl(esp_avrc_pt_cmd_t cmd) {
    if (!connected_) {
        ESP_LOGW(TAG, "Not connected to any device");
        return;
    }
    esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_PRESSED);
    esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_RELEASED);
}

void BtA2dpSink::SetVolume(uint8_t volume) {
    if (!connected_) {
        ESP_LOGW(TAG, "Not connected to any device");
        return;
    }
    volume_ = volume;
    esp_avrc_ct_send_set_absolute_volume_cmd(0, volume);
}

bool BtA2dpSink::IsConnected() const {
    return connected_;
}

bool BtA2dpSink::IsPlaying() const {
    return playing_;
}

std::string BtA2dpSink::GetConnectedDeviceName() const {
    return connected_device_name_;
}

void BtA2dpSink::GapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "authentication success: %s", param->auth_cmpl.device_name);
                GetInstance().connected_device_name_.assign(
                    reinterpret_cast<const char*>(param->auth_cmpl.device_name),
                    strlen(reinterpret_cast<const char*>(param->auth_cmpl.device_name))
                );
            } else {
                ESP_LOGE(TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
            }
            break;
        }
        default:
            break;
    }
}

void BtA2dpSink::A2dpCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {
    auto& instance = GetInstance();
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "Device connected");
                instance.connected_ = true;
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "Device disconnected");
                instance.connected_ = false;
                instance.playing_ = false;
            }
            break;
        }
        case ESP_A2D_AUDIO_STATE_EVT: {
            if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
                ESP_LOGI(TAG, "Audio playing started");
                instance.playing_ = true;
            } else if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STOPPED || 
                      param->audio_stat.state == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND) {
                ESP_LOGI(TAG, "Audio playing stopped");
                instance.playing_ = false;
            }
            break;
        }
        default:
            break;
    }
}

void BtA2dpSink::AvrcpCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param) {
    ESP_LOGD(TAG, "AVRCP event: %d", event);
}

void BtA2dpSink::AudioDataCallback(const uint8_t* buf, uint32_t len) {
    if (len > 0 && buf != nullptr) {
        // 直接将数据发送给BtAudioCodec处理，它现在是一个完整的AudioCodec
        BtAudioCodec::GetInstance().ProcessData(buf, len);
    }
}

} // namespace xiaozhi