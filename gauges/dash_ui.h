#pragma once
/*
 * gauges/dash_ui.h  —  LVGL dashboard widget construction
 *
 * Builds the full gauge widget tree on the active LVGL screen.
 * Theme colours and all widget handles live in core/themes/theme.h;
 * this file only contains create_dash_ui() which assembles the layout.
 *
 * Included by both:
 *   wsscreen-1.ino          (C++ via arduino-cli)
 *   lv_sim/src/main.c       (C via CMake)
 *
 * Usage:
 *   1. Populate g_theme (e.g. g_theme = CLUSTER_THEMES[0]).
 *   2. Call create_dash_ui() once — builds widget tree, fills widget handles.
 *   3. Call apply_theme_to_ui() any time g_theme changes.
 *   4. Drive s_speed_arc / s_rpm_label / s_speed_label / s_fuel_arc
 *      directly from the gauge-data loop or ESP-NOW callback.
 */

#include "../core/themes/theme.h"   /* g_theme, widget handles, apply_theme_to_ui */

/* create_dash_ui()
 * Builds the full widget tree on the active screen using g_theme for colours.
 * Call once, after setting g_theme, under lvgl_lock() if needed.          */
static void create_dash_ui(void)
{
    lv_obj_t *scr = lv_screen_active();

    /* Background: filled circle matching the AMOLED circular aperture.
     * Created first so it sits below all other widgets.              */
    s_bg = lv_obj_create(scr);
    lv_obj_set_size(s_bg, 466, 466);
    lv_obj_align(s_bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(s_bg, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bg, lv_color_hex(g_theme.bg_color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bg, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_bg, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_bg, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* Speed arc: 450×450, centered, 270-degree sweep (gap at bottom) */
    s_speed_arc = lv_arc_create(scr);
    lv_obj_set_size(s_speed_arc, 450, 450);
    lv_obj_center(s_speed_arc);
    lv_obj_set_style_bg_opa(s_speed_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_speed_arc, 0, LV_PART_MAIN);
    lv_arc_set_bg_angles(s_speed_arc, 135, 45);   /* 270 deg clockwise; gap at bottom */
    lv_arc_set_range(s_speed_arc, 0, 240);
    lv_arc_set_value(s_speed_arc, 0);
    lv_obj_remove_flag(s_speed_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(s_speed_arc,
        lv_color_hex(g_theme.inactive_color), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_speed_arc, 30, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_speed_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_speed_arc,
        lv_color_hex(g_theme.pri_color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_speed_arc, 30, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_speed_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_speed_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_speed_arc, 0, LV_PART_KNOB);

    /* Speed scale labels: 0 / 60 / 120 / 180 / 240 */
    static const struct { const char *txt; int16_t x; int16_t y; }
    ticks[] = {
        { "0",    -190,  90 },
        { "60",   -168, -90 },
        { "120",     0, -196 },
        { "180",   168, -90 },
        { "240",   190,  90 },
    };
    for (int i = 0; i < 5; i++) {
        lv_obj_t *lbl = lv_label_create(scr);
        lv_label_set_text(lbl, ticks[i].txt);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl,
            lv_color_hex(g_theme.subtext_color), LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, ticks[i].x, ticks[i].y);
    }

    /* RPM: dominant centre readout */
    lv_obj_t *rpm_unit = lv_label_create(scr);
    lv_label_set_text(rpm_unit, "RPM");
    lv_obj_set_style_text_font(rpm_unit, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(rpm_unit,
        lv_color_hex(g_theme.subtext_color), LV_PART_MAIN);
    lv_obj_align(rpm_unit, LV_ALIGN_CENTER, 0, -58);

    s_rpm_label = lv_label_create(scr);
    lv_label_set_text(s_rpm_label, "0");
    lv_obj_set_size(s_rpm_label, 240, 68);
    lv_obj_set_style_text_font(s_rpm_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_rpm_label,
        lv_color_hex(g_theme.text_color), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_rpm_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_rpm_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_rpm_label, 0, LV_PART_MAIN);
    lv_obj_align(s_rpm_label, LV_ALIGN_CENTER, 0, -8);

    /* Speed: secondary readout below RPM */
    s_speed_label = lv_label_create(scr);
    lv_label_set_text(s_speed_label, "0");
    lv_obj_set_size(s_speed_label, 150, 40);
    lv_obj_set_style_text_font(s_speed_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_speed_label,
        lv_color_hex(g_theme.pri_color), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_speed_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_speed_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_speed_label, 0, LV_PART_MAIN);
    lv_obj_align(s_speed_label, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t *spd_unit = lv_label_create(scr);
    lv_label_set_text(spd_unit, "km/h");
    lv_obj_set_style_text_font(spd_unit, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(spd_unit,
        lv_color_hex(g_theme.subtext_color), LV_PART_MAIN);
    lv_obj_align(spd_unit, LV_ALIGN_CENTER, 0, 94);

    /* Fuel arc: 450×450, fills the 70-degree bottom gap of the speed arc */
    s_fuel_arc = lv_arc_create(scr);
    lv_obj_set_size(s_fuel_arc, 450, 450);
    lv_obj_center(s_fuel_arc);
    lv_obj_set_style_bg_opa(s_fuel_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_fuel_arc, 0, LV_PART_MAIN);
    lv_arc_set_bg_angles(s_fuel_arc, 55, 125);         /* 70 deg centred at bottom */
    lv_arc_set_mode(s_fuel_arc, LV_ARC_MODE_REVERSE);  /* 0% = left end            */
    lv_arc_set_range(s_fuel_arc, 0, 100);
    lv_arc_set_value(s_fuel_arc, 75);
    lv_obj_remove_flag(s_fuel_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(s_fuel_arc,
        lv_color_hex(g_theme.inactive_color), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_fuel_arc, 30, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_fuel_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_fuel_arc,
        lv_color_hex(g_theme.sec_color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_fuel_arc, 30, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_fuel_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_fuel_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_fuel_arc, 0, LV_PART_KNOB);

    lv_obj_t *fuel_lbl = lv_label_create(scr);
    lv_label_set_text(fuel_lbl, "FUEL");
    lv_obj_set_style_text_font(fuel_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(fuel_lbl,
        lv_color_hex(g_theme.subtext_color), LV_PART_MAIN);
    lv_obj_align(fuel_lbl, LV_ALIGN_CENTER, 0, 190);
}
