#include "bt_a2dp_sink.h"
#include <esp_log.h>
#include <string.h>

#define TAG "BtA2dpSink"

// 全局指向单例的指针，用于静态回调函数
static BtA2dpSink* g_bt_a2dp_sink = nullptr;

BtA2dpSink::BtA2dpSink() {
    g_bt_a2dp_sink = this;
}

BtA2dpSink::~BtA2dpSink() {
    Deinit();
    g_bt_a2dp_sink = nullptr;
}

bool BtA2dpSink::Init(const std::string& device_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        ESP_LOGW(TAG, "A2DP sink already initialized");
        return true;
    }
    
    device_name_ = device_name;
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 初始化蓝牙控制器
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "Initialize controller failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(TAG, "Enable controller failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 初始化蓝牙主机
    if ((ret = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(TAG, "Initialize bluedroid failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(TAG, "Enable bluedroid failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 设置设备名称
    if ((ret = esp_bt_dev_set_device_name(device_name_.c_str())) != ESP_OK) {
        ESP_LOGE(TAG, "Set device name failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 注册GAP回调
    if ((ret = esp_bt_gap_register_callback(BtAppGapCallback)) != ESP_OK) {
        ESP_LOGE(TAG, "Register GAP callback failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 设置可发现和可连接模式
    if ((ret = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE)) != ESP_OK) {
        ESP_LOGE(TAG, "Set scan mode failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 初始化A2DP接收器
    if ((ret = esp_a2d_register_callback(BtAppA2dCallback)) != ESP_OK) {
        ESP_LOGE(TAG, "Register A2DP callback failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    if ((ret = esp_a2d_sink_register_data_callback(BtAppA2dDataCallback)) != ESP_OK) {
        ESP_LOGE(TAG, "Register A2DP data callback failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    if ((ret = esp_a2d_sink_init()) != ESP_OK) {
        ESP_LOGE(TAG, "Initialize A2DP sink failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 初始化AVRCP控制器
    if ((ret = esp_avrc_ct_init()) != ESP_OK) {
        ESP_LOGE(TAG, "Initialize AVRC controller failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    if ((ret = esp_avrc_ct_register_callback(BtAppAvrcCallback)) != ESP_OK) {
        ESP_LOGE(TAG, "Register AVRC callback failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 设置安全模式
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
    
    // 设置COD
    esp_bt_cod_t cod;
    cod.major = ESP_BT_COD_MAJOR_DEV_AUDIO;
    cod.minor = ESP_BT_COD_MINOR_AUDIO_HIFI;
    cod.service = ESP_BT_COD_SRVC_RENDERING | ESP_BT_COD_SRVC_AUDIO;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);
    
    ESP_LOGI(TAG, "A2DP sink initialized, device name: %s", device_name_.c_str());
    initialized_ = true;
    return true;
}

void BtA2dpSink::Deinit() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    // 反初始化AVRCP
    esp_avrc_ct_deinit();
    
    // 反初始化A2DP
    esp_a2d_sink_deinit();
    
    // 反初始化蓝牙
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    initialized_ = false;
    connected_ = false;
    playing_ = false;
    connected_device_name_.clear();
    memset(peer_bda_, 0, sizeof(peer_bda_));
    
    ESP_LOGI(TAG, "A2DP sink deinitialized");
}

void BtA2dpSink::SetAudioDataCallback(std::function<void(const uint8_t*, uint32_t)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    audio_data_callback_ = callback;
}

std::string BtA2dpSink::GetConnectedDeviceAddress() const {
    char addr_str[18];
    sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
            peer_bda_[0], peer_bda_[1], peer_bda_[2],
            peer_bda_[3], peer_bda_[4], peer_bda_[5]);
    return std::string(addr_str);
}

bool BtA2dpSink::PlayControl(esp_avrc_pt_cmd_t cmd) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        ESP_LOGW(TAG, "Not connected to any device");
        return false;
    }
    
    esp_err_t ret = esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_PRESSED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send passthrough command pressed failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ret = esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_RELEASED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send passthrough command released failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

void BtA2dpSink::SetVolume(uint8_t volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (volume > 100) {
        volume = 100;
    }
    
    volume_ = volume;
    
    if (connected_) {
        esp_avrc_ct_send_set_absolute_volume_cmd(0, volume);
    }
}

void BtA2dpSink::ProcessAudioData(const uint8_t* data, uint32_t len) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (audio_data_callback_) {
        audio_data_callback_(data, len);
    }
}

void BtA2dpSink::ProcessAvrcMetadata(uint8_t attr_id, const uint8_t* attr_value, uint16_t attr_length) {
    if (attr_length <= 0) {
        return;
    }
    
    // 确保字符串以null结尾
    char* str = (char*)malloc(attr_length + 1);
    if (str == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for metadata");
        return;
    }
    
    memcpy(str, attr_value, attr_length);
    str[attr_length] = '\0';
    
    switch (attr_id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            ESP_LOGI(TAG, "Title: %s", str);
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            ESP_LOGI(TAG, "Artist: %s", str);
            break;
        case ESP_AVRC_MD_ATTR_ALBUM:
            ESP_LOGI(TAG, "Album: %s", str);
            break;
        case ESP_AVRC_MD_ATTR_GENRE:
            ESP_LOGI(TAG, "Genre: %s", str);
            break;
        case ESP_AVRC_MD_ATTR_PLAYING_TIME:
            ESP_LOGI(TAG, "Playing time: %s", str);
            break;
        default:
            ESP_LOGI(TAG, "Unknown metadata: %s", str);
            break;
    }
    
    free(str);
}

void BtA2dpSink::ProcessAvrcNotification(uint8_t event_id, esp_avrc_rn_param_t* event_parameter) {
    switch (event_id) {
        case ESP_AVRC_RN_PLAY_STATUS_CHANGE:
            switch (event_parameter->play_status) {
                case ESP_AVRC_PLAYBACK_PLAYING:
                    ESP_LOGI(TAG, "Play status: Playing");
                    playing_ = true;
                    break;
                case ESP_AVRC_PLAYBACK_PAUSED:
                    ESP_LOGI(TAG, "Play status: Paused");
                    playing_ = false;
                    break;
                case ESP_AVRC_PLAYBACK_STOPPED:
                    ESP_LOGI(TAG, "Play status: Stopped");
                    playing_ = false;
                    break;
                default:
                    ESP_LOGI(TAG, "Play status: %d", event_parameter->play_status);
                    break;
            }
            break;
        case ESP_AVRC_RN_TRACK_CHANGE:
            ESP_LOGI(TAG, "Track changed");
            break;
        case ESP_AVRC_RN_VOLUME_CHANGE:
            ESP_LOGI(TAG, "Volume changed: %d", event_parameter->volume);
            volume_ = event_parameter->volume;
            break;
        default:
            ESP_LOGI(TAG, "AVRC event: %d", event_id);
            break;
    }
}

// 静态回调函数
void BtA2dpSink::BtAppGapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
    if (g_bt_a2dp_sink == nullptr) {
        return;
    }
    
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Authentication success: %s", param->auth_cmpl.device_name);
                g_bt_a2dp_sink->connected_device_name_ = std::string((char*)param->auth_cmpl.device_name);
            } else {
                ESP_LOGE(TAG, "Authentication failed, status: %d", param->auth_cmpl.stat);
            }
            break;
        }
        case ESP_BT_GAP_CFM_REQ_EVT: {
            ESP_LOGI(TAG, "ESP_BT_GAP_CFM_REQ_EVT, Please compare the numeric value: %d", param->cfm_req.num_val);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;
        }
        case ESP_BT_GAP_KEY_NOTIF_EVT: {
            ESP_LOGI(TAG, "ESP_BT_GAP_KEY_NOTIF_EVT, Passkey: %d", param->key_notif.passkey);
            break;
        }
        case ESP_BT_GAP_KEY_REQ_EVT: {
            ESP_LOGI(TAG, "ESP_BT_GAP_KEY_REQ_EVT, Please enter passkey!");
            break;
        }
        default: {
            ESP_LOGI(TAG, "GAP event: %d", event);
            break;
        }
    }
}

void BtA2dpSink::BtAppA2dCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {
    if (g_bt_a2dp_sink == nullptr) {
        return;
    }
    
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
            switch (param->conn_stat.state) {
                case ESP_A2D_CONNECTION_STATE_DISCONNECTED: {
                    ESP_LOGI(TAG, "A2DP connection state: Disconnected");
                    g_bt_a2dp_sink->connected_ = false;
                    g_bt_a2dp_sink->playing_ = false;
                    break;
                }
                case ESP_A2D_CONNECTION_STATE_CONNECTING: {
                    ESP_LOGI(TAG, "A2DP connection state: Connecting");
                    break;
                }
                case ESP_A2D_CONNECTION_STATE_CONNECTED: {
                    ESP_LOGI(TAG, "A2DP connection state: Connected, [%02x:%02x:%02x:%02x:%02x:%02x]",
                             param->conn_stat.remote_bda[0], param->conn_stat.remote_bda[1],
                             param->conn_stat.remote_bda[2], param->conn_stat.remote_bda[3],
                             param->conn_stat.remote_bda[4], param->conn_stat.remote_bda[5]);
                    g_bt_a2dp_sink->connected_ = true;
                    memcpy(g_bt_a2dp_sink->peer_bda_, param->conn_stat.remote_bda, sizeof(esp_bd_addr_t));
                    break;
                }
                case ESP_A2D_CONNECTION_STATE_DISCONNECTING: {
                    ESP_LOGI(TAG, "A2DP connection state: Disconnecting");
                    break;
                }
                default: {
                    ESP_LOGE(TAG, "Unknown A2DP connection state: %d", param->conn_stat.state);
                    break;
                }
            }
            break;
        }
        case ESP_A2D_AUDIO_STATE_EVT: {
            switch (param->audio_stat.state) {
                case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND: {
                    ESP_LOGI(TAG, "A2DP audio state: Suspended");
                    g_bt_a2dp_sink->playing_ = false;
                    break;
                }
                case ESP_A2D_AUDIO_STATE_STOPPED: {
                    ESP_LOGI(TAG, "A2DP audio state: Stopped");
                    g_bt_a2dp_sink->playing_ = false;
                    break;
                }
                case ESP_A2D_AUDIO_STATE_STARTED: {
                    ESP_LOGI(TAG, "A2DP audio state: Started");
                    g_bt_a2dp_sink->playing_ = true;
                    break;
                }
                default: {
                    ESP_LOGE(TAG, "Unknown A2DP audio state: %d", param->audio_stat.state);
                    break;
                }
            }
            break;
        }
        case ESP_A2D_AUDIO_CFG_EVT: {
            ESP_LOGI(TAG, "A2DP audio configuration, codec type: %d", param->audio_cfg.mcc.type);
            // 获取音频配置信息
            if (param->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
                ESP_LOGI(TAG, "Sample rate: %d", param->audio_cfg.mcc.cie.sbc[0] & 0x0F);
                ESP_LOGI(TAG, "Channel mode: %d", (param->audio_cfg.mcc.cie.sbc[0] >> 4) & 0x0F);
                ESP_LOGI(TAG, "Subbands: %d", (param->audio_cfg.mcc.cie.sbc[1] >> 6) & 0x01);
                ESP_LOGI(TAG, "Allocation method: %d", (param->audio_cfg.mcc.cie.sbc[1] >> 7) & 0x01);
                ESP_LOGI(TAG, "Block length: %d", (param->audio_cfg.mcc.cie.sbc[1] >> 4) & 0x03);
            }
            break;
        }
        default: {
            ESP_LOGI(TAG, "A2DP event: %d", event);
            break;
        }
    }
}

void BtA2dpSink::BtAppAvrcCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param) {
    if (g_bt_a2dp_sink == nullptr) {
        return;
    }
    
    switch (event) {
        case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
            if (param->conn_stat.connected) {
                ESP_LOGI(TAG, "AVRC connected");
                
                // 注册通知
                esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
                esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_TRACK_CHANGE, 0);
                esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_VOLUME_CHANGE, 0);
                
                // 获取播放状态
                esp_avrc_ct_send_metadata_cmd(0, ESP_AVRC_MD_ATTR_TITLE | 
                                                ESP_AVRC_MD_ATTR_ARTIST | 
                                                ESP_AVRC_MD_ATTR_ALBUM | 
                                                ESP_AVRC_MD_ATTR_GENRE);
                
                // 设置音量
                esp_avrc_ct_send_set_absolute_volume_cmd(0, g_bt_a2dp_sink->volume_);
            } else {
                ESP_LOGI(TAG, "AVRC disconnected");
            }
            break;
        }
        case ESP_AVRC_CT_METADATA_RSP_EVT: {
            ESP_LOGI(TAG, "AVRC metadata response: attribute id 0x%x, %d bytes", 
                     param->meta_rsp.attr_id, param->meta_rsp.attr_length);
            g_bt_a2dp_sink->ProcessAvrcMetadata(param->meta_rsp.attr_id, 
                                               param->meta_rsp.attr_text, 
                                               param->meta_rsp.attr_length);
            break;
        }
        case ESP_AVRC_CT_CHANGE_NOTIFY_EVT: {
            ESP_LOGI(TAG, "AVRC event notification: %d", param->change_ntf.event_id);
            g_bt_a2dp_sink->ProcessAvrcNotification(param->change_ntf.event_id, 
                                                   &param->change_ntf.event_parameter);
            
            // 重新注册通知
            esp_avrc_ct_send_register_notification_cmd(0, param->change_ntf.event_id, 0);
            break;
        }
        case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT: {
            ESP_LOGI(TAG, "Set absolute volume response: %d", param->set_volume_rsp.volume);
            g_bt_a2dp_sink->volume_ = param->set_volume_rsp.volume;
            break;
        }
        default: {
            ESP_LOGI(TAG, "AVRC event: %d", event);
            break;
        }
    }
}

void BtA2dpSink::BtAppA2dDataCallback(const uint8_t* data, uint32_t len) {
    if (g_bt_a2dp_sink == nullptr) {
        return;
    }
    
    g_bt_a2dp_sink->ProcessAudioData(data, len);
} 