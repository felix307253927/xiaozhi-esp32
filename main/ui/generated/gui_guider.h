/*
 * @Author             : Felix
 * @Email              : 307253927@qq.com
 * @Date               : 2025-03-20 21:57:44
 * @LastEditors        : Felix
 * @LastEditTime       : 2025-03-20 22:15:25
 */
/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"


typedef struct
{
  
	lv_obj_t *home_analog;
	bool home_analog_del;
	lv_obj_t *home_analog_analog_clock_1;
	lv_obj_t *home_analog_analog_clock_1_hour_needle;
	lv_obj_t *home_analog_analog_clock_1_min_needle;
	lv_obj_t *home_analog_analog_clock_1_sec_needle;
	lv_obj_t *home;
	bool home_del;
	lv_obj_t *home_chat_message_label_;
	lv_obj_t *home_emotion_label_;
	lv_obj_t *home_notification_label_;
	lv_obj_t *home_network_label_;
	lv_obj_t *home_status_label_;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_screen_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, uint32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                  uint32_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                  lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_completed_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_bottom_layer(void);

void setup_ui(lv_ui *ui);

void video_play(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_home_analog(lv_ui *ui);
void setup_scr_home(lv_ui *ui);

LV_IMAGE_DECLARE(_lock_RGB565A8_360x360);
LV_IMAGE_DECLARE(_img_clockwise_hour_RGB565A8_16x60);
LV_IMAGE_DECLARE(_img_clockwise_min_RGB565A8_16x106);
LV_IMAGE_DECLARE(_img_clockwise_sec_RGB565A8_22x150);

LV_IMAGE_DECLARE(_home_RGB565A8_360x360);

// LV_FONT_DECLARE(lv_font_montserratMedium_15)
// LV_FONT_DECLARE(lv_font_montserratMedium_24)


#ifdef __cplusplus
}
#endif
#endif
