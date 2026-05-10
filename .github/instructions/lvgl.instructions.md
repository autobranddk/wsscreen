---
description: "Use when writing, editing, or reviewing LVGL UI code. Covers LVGL 9.x API conventions: widget creation, styles, events, arcs, labels, animations, layouts, thread safety, and lv_conf.h settings. Trigger on: lv_obj, lv_style, lv_arc, lv_label, lv_event, LVGL, lvgl.h, dash_ui, create_dash_ui, apply_theme."
applyTo: ["**/*.h", "**/*.c", "**/*.cpp", "**/*.ino"]
---

# LVGL 9.x Coding Rules

Documentation: https://lvgl.io/docs/open  
API reference: https://lvgl.io/docs/open/api  
Widgets: https://lvgl.io/docs/open/widgets  
Common widget features (styles, events, layouts): https://lvgl.io/docs/open/common-widget-features  

## API Version

Always use **LVGL v9.x** API. This project targets v9.2.

| Deprecated (v8) | Correct (v9) |
|---|---|
| `lv_disp_t` | `lv_display_t` |
| `lv_indev_drv_t` | `lv_indev_t` |
| `lv_scr_act()` | `lv_screen_active()` |
| `lv_disp_get_scr_act()` | `lv_screen_active()` |
| `lv_obj_set_style_local_*` | `lv_obj_set_style_*(..., LV_PART_MAIN)` |

## Widget Creation

- Create widgets with `lv_<widget>_create(parent)`.
- Always pass `lv_screen_active()` (or a container object) as the parent — never `NULL`.
- Delete widgets with `lv_obj_delete(obj)` (not `lv_obj_del` — renamed in v9).

## Styles

- Prefer **inline styles** (`lv_obj_set_style_*`) for per-widget overrides; use `lv_style_t` only for shared/reusable styles.
- Always specify a part and state: `lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT)`.
- After bulk inline-style changes call `lv_obj_refresh_style(obj, LV_PART_ANY, LV_STYLE_PROP_ANY)` to force a redraw.
- Arc indicator color → `LV_PART_INDICATOR`; track color → `LV_PART_MAIN`.

## Events

- Register callbacks with `lv_obj_add_event_cb(obj, cb, LV_EVENT_*, user_data)`.
- Inside a callback retrieve the target with `lv_event_get_target(e)` and user-data with `lv_event_get_user_data(e)`.
- Remove a specific callback with `lv_obj_remove_event_cb(obj, cb)`.

## Thread Safety (FreeRTOS)

This project runs LVGL on a dedicated FreeRTOS task protected by `s_lvgl_mux`.  
**Any LVGL call from a different task must be wrapped:**

```c
if (xSemaphoreTake(s_lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
    // LVGL calls here
    xSemaphoreGive(s_lvgl_mux);
}
```

Never call `lv_*` functions from ESP-NOW callbacks or other ISR/task contexts without acquiring the mutex first.

## Colors

- Use `lv_color_hex(0xRRGGBB)` for hex literals (project themes store colors as `uint32_t` hex values).
- Use `lv_color_white()` / `lv_color_black()` for pure white/black.
- Opacity: `lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN)` — use `LV_OPA_*` constants.

## Layouts (Flexbox / Grid)

- Prefer `lv_obj_set_layout(obj, LV_LAYOUT_FLEX)` with `lv_obj_set_flex_flow` / `lv_obj_set_flex_align` over manual positioning where possible.
- For fixed positioning use `lv_obj_set_pos` / `lv_obj_align` / `lv_obj_align_to`.

## Fonts

- Declare fonts used in `lv_conf.h` with `LV_FONT_*` macros before referencing them.
- Assign with `lv_obj_set_style_text_font(obj, &lv_font_montserrat_XX, LV_PART_MAIN)`.

## Animations

- Use `lv_anim_t` + `lv_anim_start()`; do NOT busy-loop or use `vTaskDelay` to simulate animation.
- Free animations automatically clean up; do not manually free `lv_anim_t` after `lv_anim_start`.
