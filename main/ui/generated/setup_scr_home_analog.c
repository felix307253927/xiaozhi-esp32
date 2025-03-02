/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"



int home_analog_analog_clock_1_hour_value = 0;
int home_analog_analog_clock_1_min_value = 0;
int home_analog_analog_clock_1_sec_value = 0;
void setup_scr_home_analog(lv_ui *ui)
{
    //Write codes home_analog
    ui->home_analog = lv_obj_create(NULL);
    lv_obj_set_size(ui->home_analog, 128, 128);
    lv_obj_set_scrollbar_mode(ui->home_analog, LV_SCROLLBAR_MODE_OFF);

    //Write style for home_analog, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    // lv_obj_set_style_bg_opa(ui->home_analog, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->home_analog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_dir(ui->home_analog, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui->home_analog, &_img_bg_analog_RGB565A8_128x160, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_opa(ui->home_analog, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_recolor_opa(ui->home_analog, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_analog_analog_clock_1
    static bool home_analog_analog_clock_1_timer_enabled = false;
    static const char * home_analog_analog_clock_1_hour_ticks[] = {"12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", NULL};
    ui->home_analog_analog_clock_1 = lv_scale_create(ui->home_analog);
    lv_obj_set_pos(ui->home_analog_analog_clock_1, 5, 5);
    lv_obj_set_size(ui->home_analog_analog_clock_1, 118, 118);
    lv_scale_set_mode(ui->home_analog_analog_clock_1, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_angle_range(ui->home_analog_analog_clock_1, 360U);
    lv_scale_set_range(ui->home_analog_analog_clock_1, 0U, 60U);
    lv_scale_set_rotation(ui->home_analog_analog_clock_1, 270U);
    lv_obj_set_style_radius(ui->home_analog_analog_clock_1, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(ui->home_analog_analog_clock_1, true, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui->home_analog_analog_clock_1, 0, LV_PART_MAIN);

    lv_scale_set_total_tick_count(ui->home_analog_analog_clock_1, 60);

    lv_scale_set_major_tick_every(ui->home_analog_analog_clock_1, 5);
    lv_scale_set_text_src(ui->home_analog_analog_clock_1, home_analog_analog_clock_1_hour_ticks);
    lv_obj_update_layout(ui->home_analog_analog_clock_1);
    ui->home_analog_analog_clock_1_hour_needle = lv_image_create(ui->home_analog_analog_clock_1);
    lv_image_set_src(ui->home_analog_analog_clock_1_hour_needle, &_img_clockwise_hour_RGB565A8_16x92);
    lv_obj_align(ui->home_analog_analog_clock_1_hour_needle, LV_ALIGN_CENTER, 8 - 10, 46 - 0);
    lv_image_set_pivot(ui->home_analog_analog_clock_1_hour_needle, 10, 0);
    lv_scale_set_image_needle_value(ui->home_analog_analog_clock_1, ui->home_analog_analog_clock_1_hour_needle, home_analog_analog_clock_1_hour_value * 5);
    ui->home_analog_analog_clock_1_min_needle = lv_image_create(ui->home_analog_analog_clock_1);
    lv_image_set_src(ui->home_analog_analog_clock_1_min_needle, &_img_clockwise_min_RGB565A8_16x106);
    lv_obj_align(ui->home_analog_analog_clock_1_min_needle, LV_ALIGN_CENTER, 8 - 10, 53 - 0);
    lv_image_set_pivot(ui->home_analog_analog_clock_1_min_needle, 10, 0);
    lv_scale_set_image_needle_value(ui->home_analog_analog_clock_1, ui->home_analog_analog_clock_1_min_needle, home_analog_analog_clock_1_min_value);
    ui->home_analog_analog_clock_1_sec_needle = lv_image_create(ui->home_analog_analog_clock_1);
    lv_image_set_src(ui->home_analog_analog_clock_1_sec_needle, &_img_clockwise_sec_RGB565A8_22x136);
    lv_obj_align(ui->home_analog_analog_clock_1_sec_needle, LV_ALIGN_CENTER, 11 - 10, 68 - 18);
    lv_image_set_pivot(ui->home_analog_analog_clock_1_sec_needle, 10, 18);
    lv_scale_set_image_needle_value(ui->home_analog_analog_clock_1, ui->home_analog_analog_clock_1_sec_needle, home_analog_analog_clock_1_sec_value);
    // create timer
    if (!home_analog_analog_clock_1_timer_enabled) {
        lv_timer_create(home_analog_analog_clock_1_timer, 1000, NULL);
        home_analog_analog_clock_1_timer_enabled = true;
    }

    //Write style for home_analog_analog_clock_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->home_analog_analog_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_analog_analog_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for home_analog_analog_clock_1, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->home_analog_analog_clock_1, lv_color_hex(0xededed), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_analog_analog_clock_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(ui->home_analog_analog_clock_1, 0, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_length(ui->home_analog_analog_clock_1, 0, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for home_analog_analog_clock_1, Part: LV_PART_ITEMS, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->home_analog_analog_clock_1, 0, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_length(ui->home_analog_analog_clock_1, 0, LV_PART_ITEMS|LV_STATE_DEFAULT);

    //The custom code of home_analog.


    //Update current screen layout.
    lv_obj_update_layout(ui->home_analog);

    //Init events for screen.
    events_init_home_analog(ui);
}
