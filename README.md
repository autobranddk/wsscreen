# wsscreen-1 — Waveshare ESP32-S3-Touch-AMOLED-1.43 + LVGL 9.2

Arduino sketch for the **Waveshare ESP32-S3-Touch-AMOLED-1.43** development board,
featuring a 466×466 AMOLED display driven by the **SH8601** controller over QSPI,
and a **FT3168** (FT5x06-compatible) capacitive touch controller over I2C.

---

## Hardware

| Parameter | Value |
|-----------|-------|
| Display IC | SH8601 (some boards ship with CO5300) |
| Interface | QSPI (4-wire SPI) |
| Resolution | 466 × 466 pixels |
| Touch IC | FT3168 (FT5x06-compatible) |
| Touch bus | I2C |
| MCU | ESP32-S3 |

### Verified GPIO pin assignments

| Signal | GPIO |
|--------|------|
| QSPI SCLK | 10 |
| QSPI IO0 (MOSI) | 11 |
| QSPI IO1 | 12 |
| QSPI IO2 | 13 |
| QSPI IO3 | 14 |
| LCD CS | 9 |
| LCD RST | 21 |
| **LCD EN** | **42** ← must be HIGH before init |
| Touch SCL | 48 |
| Touch SDA | 47 |
| Touch RST | not wired |
| Touch INT | not wired |
| Backlight | none (brightness via DCS command 0x51) |

---

## Required libraries

Install via **Arduino IDE → Library Manager** or `arduino-cli`:

| Library | Version |
|---------|---------|
| `esp-arduino-libs/ESP32_Display_Panel` | v1.0.4 |
| `lvgl/lvgl` | 9.2.x |

---

## Project files

```
wsscreen-1/
├── wsscreen-1.ino                  ← main sketch
├── esp_panel_board_custom_conf.h   ← board GPIO config (read by ESP32_Display_Panel)
├── lv_conf.h                       ← LVGL 9 configuration
└── README.md                       ← this file
```

The two `.h` files **must** remain in the same folder as `wsscreen-1.ino` so the
libraries pick them up automatically.

---

## Arduino IDE board settings

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| PSRAM | OPI PSRAM |
| Flash Size | 16MB (or match your board) |
| Partition Scheme | 16M Flash (3MB APP / 9.9MB FATFS) |
| Upload Speed | 921600 |

---

## How it works

1. **GPIO 42 (EN) is driven HIGH** before `board->begin()` to power the display
   panel.  This is done explicitly in `setup()` via `gpio_config()`.

2. **ESP32_Display_Panel** reads `esp_panel_board_custom_conf.h` and initialises
   the SH8601 via the custom vendor init sequence defined there.  The default
   `esp_lcd_sh8601` driver is missing the mandatory **Sleep Out (0x11)** command;
   the custom sequence adds it with a 120 ms delay.

3. **LVGL 9** is initialised with two DMA-capable SRAM buffers
   (`MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`).  PSRAM is intentionally avoided for
   display buffers because the ESP32-S3's QSPI DMA cannot read from PSRAM at
   40 MHz without underflow errors.

4. A **FreeRTOS task** runs `lv_timer_handler()` in a loop, protected by a
   recursive mutex.  Call `lvgl_lock()` / `lvgl_unlock()` before/after any LVGL
   API call made outside that task.

5. The **invalidation-area callback** rounds all dirty regions to even
   coordinates, which is required by the SH8601 / CO5300 CASET/PASET protocol.

---

## CO5300 variant

Some units ship with a **CO5300** instead of SH8601 (same form factor, different
chip ID).  If the display stays black:

1. Check which IC is fitted (see the PCB silkscreen near the display connector).
2. Replace the `ESP_PANEL_BOARD_LCD_VENDOR_INIT_CMD()` sequence in
   `esp_panel_board_custom_conf.h` with:

```c
#define ESP_PANEL_BOARD_LCD_VENDOR_INIT_CMD()                                    \
    {                                                                             \
        {0x11, (uint8_t []){0x00},       0,   80}, /* Sleep Out             */  \
        {0xC4, (uint8_t []){0x80},       1,    0}, /* Display mode config    */  \
        {0x53, (uint8_t []){0x20},       1,    1}, /* Write CTRL display     */  \
        {0x63, (uint8_t []){0xFF},       1,    1}, /* Brightness control ext */  \
        {0x51, (uint8_t []){0x00},       1,    1}, /* Brightness = 0         */  \
        {0x29, (uint8_t []){0x00},       0,   10}, /* Display ON             */  \
        {0x51, (uint8_t []){0xFF},       1,    0}, /* Brightness = max       */  \
    }
```

Also change the controller line to match (CO5300 is not a named controller in
ESP32_Display_Panel, so keep `SH8601` — the command set is compatible enough for
basic rendering):

```
// The CO5300 shares the same QSPI command framing as SH8601; only the
// initialisation sequence differs.  The SH8601 driver type is reused here.
```

---

## Brightness control at runtime

The AMOLED backlight is controlled entirely by the MIPI DCS brightness register
(command `0x51`).  To adjust brightness at runtime:

```cpp
// level: 0x00 (off) ... 0xFF (max)
void set_brightness(uint8_t level) {
    auto *io = s_board->getLCD()->getPanelIO();  // esp_lcd_panel_io_handle_t
    // SH8601 QSPI command format: (0x02 << 24) | (cmd << 8)
    esp_lcd_panel_io_tx_param(io, (0x02 << 24) | (0x51 << 8), &level, 1);
}
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Display stays black | EN pin not HIGH / Sleep Out missing | Check GPIO 42; verify vendor init |
| Garbled / shifted rendering | Odd draw coordinates | Ensure invalidate-area callback is registered |
| Touch unresponsive | Wrong I2C address | Try `0x38`; verify SCL/SDA |
| Build error: `Board not found` | Custom conf not loaded | Place `esp_panel_board_custom_conf.h` beside `.ino` |
| DMA underflow / noise | PSRAM used for buffers | Use `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL` |
