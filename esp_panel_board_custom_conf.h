/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file  esp_panel_board_custom_conf.h
 * @brief Custom board configuration for Waveshare ESP32-S3-Touch-AMOLED-1.43
 *
 * Display:  SH8601 AMOLED, 466x466 pixels, QSPI interface
 * Touch:    FT3168 (FT5x06-compatible), I2C
 *
 * Verified GPIO pin assignments (from community testing):
 *   QSPI SCLK : GPIO 10    Touch SCL : GPIO 48
 *   QSPI IO0  : GPIO 11    Touch SDA : GPIO 47
 *   QSPI IO1  : GPIO 12    Touch RST : not connected (-1)
 *   QSPI IO2  : GPIO 13    Touch INT : not connected (-1)
 *   QSPI IO3  : GPIO 14
 *   LCD CS    : GPIO 9
 *   LCD RST   : GPIO 21
 *   LCD EN    : GPIO 42  <-- drive HIGH before board->begin()
 *   Backlight : none (brightness via MIPI DCS command 0x51)
 *
 * NOTE: Some boards ship with a CO5300 instead of SH8601.
 *       If the display stays black, try the CO5300 init sequence
 *       (see README.md for details).
 */

#pragma once

// *INDENT-OFF*

// --------------------------------------------------------------------------
// Enable custom board configuration
// --------------------------------------------------------------------------
#define ESP_PANEL_BOARD_DEFAULT_USE_CUSTOM  (1)

#if ESP_PANEL_BOARD_DEFAULT_USE_CUSTOM

// --------------------------------------------------------------------------
// General parameters
// --------------------------------------------------------------------------
#define ESP_PANEL_BOARD_NAME    "Waveshare:ESP32-S3-Touch-AMOLED-1.43"
#define ESP_PANEL_BOARD_WIDTH   (480)  // DDRAM is 480 wide; circular aperture ~466px
#define ESP_PANEL_BOARD_HEIGHT  (466)

// --------------------------------------------------------------------------
// LCD panel configuration
// --------------------------------------------------------------------------
#define ESP_PANEL_BOARD_USE_LCD         (1)

#if ESP_PANEL_BOARD_USE_LCD

// SH8601 AMOLED controller via QSPI
#define ESP_PANEL_BOARD_LCD_CONTROLLER  SH8601
#define ESP_PANEL_BOARD_LCD_BUS_TYPE    (ESP_PANEL_BUS_TYPE_QSPI)

#define ESP_PANEL_BOARD_LCD_BUS_SKIP_INIT_HOST  (0)

// QSPI bus host (SPI2_HOST = 1)
#define ESP_PANEL_BOARD_LCD_QSPI_HOST_ID    (1)

// QSPI bus pins
#define ESP_PANEL_BOARD_LCD_QSPI_IO_SCK     (10)
#define ESP_PANEL_BOARD_LCD_QSPI_IO_DATA0   (11)
#define ESP_PANEL_BOARD_LCD_QSPI_IO_DATA1   (12)
#define ESP_PANEL_BOARD_LCD_QSPI_IO_DATA2   (13)
#define ESP_PANEL_BOARD_LCD_QSPI_IO_DATA3   (14)

// Panel control pins (on the QSPI bus)
#define ESP_PANEL_BOARD_LCD_QSPI_IO_CS      (9)
#define ESP_PANEL_BOARD_LCD_QSPI_MODE       (0)
#define ESP_PANEL_BOARD_LCD_QSPI_CLK_HZ     (40 * 1000 * 1000)  // 40 MHz
#define ESP_PANEL_BOARD_LCD_QSPI_CMD_BITS   (32)  // SH8601 QSPI command is 32-bit
#define ESP_PANEL_BOARD_LCD_QSPI_PARAM_BITS (8)

/**
 * Custom SH8601 initialization sequence.
 *
 * CRITICAL: The default esp_lcd_sh8601 driver does NOT include Sleep Out
 * (0x11). Without it the panel stays in sleep mode and shows nothing.
 *
 * Sequence verified on the Waveshare ESP32-S3-Touch-AMOLED-1.43 board.
 * Format: {cmd, data_ptr, data_len, delay_ms}
 *
 *  0x11  Sleep Out            -- mandatory, 120 ms settle time
 *  0x44  Set Tear Scanline    -- y=0x01D1=465 (last line)
 *  0x35  Tearing Effect ON    -- mode 0 = V-blank only
 *  0x53  Write CTRL Display   -- 0x20 = backlight on via WDBRIGHTNESS
 *  0x51  Write Display Brightness -- start at 0 (ramp up after init)
 *  0x29  Display ON           -- 10 ms settle
 *  0x51  Write Display Brightness -- 0xFF = max brightness
 */
#define ESP_PANEL_BOARD_LCD_VENDOR_INIT_CMD()                                    \
    {                                                                             \
        {0x11, (uint8_t []){0x00},       0,  120}, /* Sleep Out, wait 120 ms */ \
        {0xFE, (uint8_t []){0x00},       1,    0}, /* CO5300: access page 0   */ \
        {0xC4, (uint8_t []){0x80},       1,    0}, /* CO5300: QSPI mode ON    */ \
        {0x44, (uint8_t []){0x01, 0xD1}, 2,    0}, /* Set tear scanline y=465 */ \
        {0x35, (uint8_t []){0x00},       1,    0}, /* Tearing effect line ON  */ \
        {0x53, (uint8_t []){0x20},       1,   10}, /* Write CTRL display      */ \
        {0x51, (uint8_t []){0x00},       1,   10}, /* Brightness = 0 (ramp)   */ \
        {0x29, (uint8_t []){0x00},       0,   10}, /* Display ON              */ \
        {0x51, (uint8_t []){0xFF},       1,    0}, /* Brightness = max        */ \
    }

// Color format
#define ESP_PANEL_BOARD_LCD_COLOR_BITS      (ESP_PANEL_LCD_COLOR_BITS_RGB565)
#define ESP_PANEL_BOARD_LCD_COLOR_BGR_ORDER (0)   // RGB order
#define ESP_PANEL_BOARD_LCD_COLOR_INEVRT_BIT (0)  // No color inversion

// Coordinate transformation (none for this board)
#define ESP_PANEL_BOARD_LCD_SWAP_XY     (0)
#define ESP_PANEL_BOARD_LCD_MIRROR_X    (0)
#define ESP_PANEL_BOARD_LCD_MIRROR_Y    (0)
#define ESP_PANEL_BOARD_LCD_GAP_X       (0)
#define ESP_PANEL_BOARD_LCD_GAP_Y       (0)

// LCD reset pin (active low)
#define ESP_PANEL_BOARD_LCD_RST_IO      (21)
#define ESP_PANEL_BOARD_LCD_RST_LEVEL   (0)

#endif // ESP_PANEL_BOARD_USE_LCD

// --------------------------------------------------------------------------
// Touch panel configuration
// --------------------------------------------------------------------------
// Touch is disabled here to work around a GCC C++20 parser bug in the
// library's esp_panel_board_default_config.cpp (std::optional<T> member
// initialisation via designated initialisers with a qualified type name).
// The FT3168 / FT5x06 driver is initialised directly in wsscreen-1.ino.
#define ESP_PANEL_BOARD_USE_TOUCH   (0)

// --------------------------------------------------------------------------
// Backlight
// --------------------------------------------------------------------------
// The SH8601 AMOLED backlight is controlled via MIPI DCS command 0x51
// (Write Display Brightness), not a GPIO pin.  Leave backlight disabled here.
// Brightness is set inside the vendor init sequence above and can be
// changed at runtime via esp_lcd_panel_io_tx_param().
#define ESP_PANEL_BOARD_USE_BACKLIGHT   (0)

// --------------------------------------------------------------------------
// IO Expander -- not used on this board
// --------------------------------------------------------------------------
#define ESP_PANEL_BOARD_USE_EXPANDER    (0)

// --------------------------------------------------------------------------
// File version -- must match the installed ESP32_Display_Panel library
// --------------------------------------------------------------------------
#define ESP_PANEL_BOARD_CUSTOM_FILE_VERSION_MAJOR 1
#define ESP_PANEL_BOARD_CUSTOM_FILE_VERSION_MINOR 2
#define ESP_PANEL_BOARD_CUSTOM_FILE_VERSION_PATCH 0

#endif // ESP_PANEL_BOARD_DEFAULT_USE_CUSTOM

// *INDENT-ON*
