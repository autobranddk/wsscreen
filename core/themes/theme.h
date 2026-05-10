#pragma once
/*
 * core/themes/theme.h  —  Runtime theme state and UI theme application
 *
 * Owns the active theme instance (g_theme) and the LVGL widget handles
 * that respond to theme changes.  Also provides apply_theme_to_ui() which
 * re-paints all themed widgets to match the current g_theme.
 *
 * Prerequisites (include before this header):
 *   lvgl.h          — lv_obj_t, lv_color_hex(), lv_obj_set_style_*
 *   (styles.h is included automatically below)
 *
 * Usage:
 *   1. Set g_theme (e.g. g_theme = CLUSTER_THEMES[0]).
 *   2. Call create_dash_ui() → builds widget tree using g_theme colours.
 *   To change theme: edit core/themes/styles.h (uncomment the desired
 *   palette), recompile and flash.
 */

#include "styles.h"    /* cluster_theme_t, CLUSTER_THEMES[], g_theme */
#include <lvgl.h>      /* lv_obj_t, style API                        */

/* ── Active theme ───────────────────────────────────────────────────────── */
/* Populate before create_dash_ui(); update + call apply_theme_to_ui() on change. */
static cluster_theme_t g_theme;   /* zero-initialised by default */

/* ── Widget handles ─────────────────────────────────────────────────────── */
/* Written by create_dash_ui(), read by the gauge loop. */
static lv_obj_t *s_bg          = NULL;  /* full-screen background circle    */
static lv_obj_t *s_speed_arc   = NULL;  /* outer speed ring                 */
static lv_obj_t *s_rpm_label   = NULL;  /* dominant RPM readout             */
static lv_obj_t *s_speed_label = NULL;  /* speed number in km/h             */
static lv_obj_t *s_fuel_arc    = NULL;  /* bottom fuel indicator            */
