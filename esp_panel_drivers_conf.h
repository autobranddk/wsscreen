/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file  esp_panel_drivers_conf.h
 * @brief ESP Panel driver configuration for wsscreen-1 sketch.
 *
 * Placed in the sketch folder so it is found first on the include path.
 * We only need QSPI (LCD) and I2C (FT3168 touch) buses, plus the FT5x06
 * touch driver.  Everything else is disabled to keep code size small.
 */

#pragma once

// *INDENT-OFF*

// --------------------------------------------------------------------------
// Bus drivers
// --------------------------------------------------------------------------
#define ESP_PANEL_DRIVERS_BUS_USE_ALL                   (0)
#define ESP_PANEL_DRIVERS_BUS_USE_SPI                   (0)
#define ESP_PANEL_DRIVERS_BUS_USE_QSPI                  (1)   // SH8601 LCD
#define ESP_PANEL_DRIVERS_BUS_USE_RGB                   (0)
#define ESP_PANEL_DRIVERS_BUS_USE_I2C                   (1)   // FT3168 touch
#define ESP_PANEL_DRIVERS_BUS_USE_MIPI_DSI              (0)

#define ESP_PANEL_DRIVERS_BUS_COMPILE_UNUSED_DRIVERS    (0)

// --------------------------------------------------------------------------
// LCD drivers
// --------------------------------------------------------------------------
#define ESP_PANEL_DRIVERS_LCD_USE_ALL                   (0)
#define ESP_PANEL_DRIVERS_LCD_USE_SH8601                (1)   // AMOLED controller
#define ESP_PANEL_DRIVERS_LCD_COMPILE_UNUSED_DRIVERS    (0)

// --------------------------------------------------------------------------
// Touch drivers
// --------------------------------------------------------------------------
#define ESP_PANEL_DRIVERS_TOUCH_MAX_POINTS              (5)
#define ESP_PANEL_DRIVERS_TOUCH_MAX_BUTTONS             (5)

#define ESP_PANEL_DRIVERS_TOUCH_USE_ALL                 (0)
#define ESP_PANEL_DRIVERS_TOUCH_USE_FT5x06              (1)   // FT3168 / FT5x06 compatible
#define ESP_PANEL_DRIVERS_TOUCH_COMPILE_UNUSED_DRIVERS  (0)

// --------------------------------------------------------------------------
// IO Expander -- not used
// --------------------------------------------------------------------------
#define ESP_PANEL_DRIVERS_EXPANDER_USE_ALL              (0)

// --------------------------------------------------------------------------
// Backlight -- controlled via DCS 0x51, not a driver
// --------------------------------------------------------------------------
#define ESP_PANEL_DRIVERS_BACKLIGHT_USE_ALL             (0)
#define ESP_PANEL_DRIVERS_BACKLIGHT_COMPILE_UNUSED_DRIVERS (0)

// --------------------------------------------------------------------------
// File version -- must match the installed ESP32_Display_Panel library v1.0.4
// --------------------------------------------------------------------------
#define ESP_PANEL_DRIVERS_CONF_FILE_VERSION_MAJOR 1
#define ESP_PANEL_DRIVERS_CONF_FILE_VERSION_MINOR 1
#define ESP_PANEL_DRIVERS_CONF_FILE_VERSION_PATCH 0

// *INDENT-ON*
