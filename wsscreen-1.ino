/**
 * wsscreen-1.ino
 *
 * LVGL 9.2 demo for the Waveshare ESP32-S3-Touch-AMOLED-1.43
 * (SH8601 AMOLED, 466x466, QSPI + FT3168 touch, I2C)
 *
 * Required Arduino libraries (install via Library Manager):
 *   - esp-arduino-libs/ESP32_Display_Panel  v1.0.4
 *   - lvgl/lvgl                             v9.2.x
 *
 * Board settings (Arduino IDE):
 *   Board:           ESP32S3 Dev Module
 *   PSRAM:           OPI PSRAM
 *   Partition:       16M Flash (3MB APP / 9.9MB FATFS) or similar
 *   Upload Speed:    921600
 *
 * Configuration files (must be in this sketch folder):
 *   esp_panel_board_custom_conf.h  -- GPIO pin assignments for this board
 *   lv_conf.h                      -- LVGL configuration
 */

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <drivers/touch/esp_panel_touch_ft5x06.hpp>
#include <lvgl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/i2c.h"          // Raw I2C API (old driver, matches ESP32_Display_Panel)
#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"
#define CLUSTER_DEFINE_THEMES
#include "core/cluster_types.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// ---------------------------------------------------------------------------
// Board / display constants
// ---------------------------------------------------------------------------
#define LCD_WIDTH           480      // SH8601/CO5300 DDRAM is 480 wide; circular aperture ~466px
#define LCD_HEIGHT          466
#define LCD_EN_GPIO         42      // Display enable pin -- drive HIGH before init
// FT3168 / FT5x06 touch (I2C, not managed by Board -- see comment in custom conf)
#define TOUCH_SCL_GPIO  48
#define TOUCH_SDA_GPIO  47
#define TOUCH_I2C_ADDR  0x38
// LVGL render buffer height in rows.
// Using 1/10 of screen height to balance memory vs. FPS.
// MUST be allocated from DMA-capable SRAM; PSRAM DMA underflows at 40 MHz QSPI.
#define LVGL_BUF_HEIGHT     (LCD_HEIGHT / 10)   // 46 rows
#define LVGL_BUF_BYTES      (LCD_WIDTH * LVGL_BUF_HEIGHT * sizeof(uint16_t))

#define LVGL_TICK_MS        2       // LVGL tick period in ms
#define LVGL_TASK_STACK     12288   // LVGL handler task stack size
#define LVGL_TASK_PRIORITY  2

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static Board              *s_board    = nullptr;
static lv_display_t       *s_display  = nullptr;
static SemaphoreHandle_t   s_lvgl_mux = nullptr;
static TouchFT5x06        *s_touch    = nullptr;  // direct FT5x06 instance
static volatile uint32_t   s_frame_count = 0;
static volatile int        s_speed_target = 0;  /* rounded-to-10 target for smooth arc */

/* Set by ESP-NOW theme callback; applied under lvgl_lock in loop() */

// ---------------------------------------------------------------------------
// LVGL / ESP32_Display_Panel integration
// ---------------------------------------------------------------------------

/**
 * LVGL flush callback.
 *
 * Transfers the rendered pixel buffer to the display via QSPI DMA.
 * drawBitmap() is called with timeout=-1 (blocking): it waits internally
 * for the DMA-done semaphore before returning, so we can safely call
 * lv_display_flush_ready() right after without an extra ISR callback.
 */
static void lvgl_flush_cb(lv_display_t *disp,
                          const lv_area_t *area,
                          uint8_t *px_map)
{
    LCD *lcd = static_cast<LCD *>(lv_display_get_user_data(disp));
    lcd->drawBitmap(area->x1,
                    area->y1,
                    area->x2 - area->x1 + 1,
                    area->y2 - area->y1 + 1,
                    reinterpret_cast<const uint8_t *>(px_map),
                    -1);   // -1 = block until DMA done (portMAX_DELAY)
    lv_display_flush_ready(disp);
    s_frame_count++;
}

/**
 * Raw two-transaction I2C read for FT3168.
 *
 * FT3168 does NOT support repeated-START reads.  esp_lcd_panel_io_rx_param
 * uses i2c_master_write_read_device() (repeated-START) and therefore always
 * fails on this IC.  Use two separate transactions instead:
 *   1) WRITE  reg address (+ STOP)
 *   2) READ   data bytes  (+ STOP)
 */
static esp_err_t ft3168_read(uint8_t reg, uint8_t *buf, size_t len)
{
    esp_err_t ret = i2c_master_write_to_device(
        I2C_NUM_0, TOUCH_I2C_ADDR, &reg, 1, pdMS_TO_TICKS(50));
    if (ret != ESP_OK) return ret;
    return i2c_master_read_from_device(
        I2C_NUM_0, TOUCH_I2C_ADDR, buf, len, pdMS_TO_TICKS(50));
}

/**
 * Read first touch point from FT3168.  Returns true and fills *x/*y when a
 * finger is detected; returns false when no touch or read error.
 */
static bool ft3168_read_touch(int16_t *x, int16_t *y)
{
    uint8_t pts = 0;
    if (ft3168_read(0x02 /* TD_STATUS */, &pts, 1) != ESP_OK) return false;
    if (pts == 0 || pts > 5) return false;
    uint8_t d[4];
    if (ft3168_read(0x03 /* TOUCH1_XH */, d, 4) != ESP_OK) return false;
    uint8_t event = (d[0] >> 6) & 0x03;   // 0=press, 1=release, 2=contact
    if (event == 1) return false;           // finger lifted
    *x = static_cast<int16_t>(((d[0] & 0x0F) << 8) | d[1]);
    *y = static_cast<int16_t>(((d[2] & 0x0F) << 8) | d[3]);
    return true;
}

/**
 * LVGL touch read callback.
 *
 * Uses ft3168_read_touch() (raw two-transaction I2C) instead of the panel_io
 * abstraction which uses repeated-START and is not supported by FT3168.
 */
static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    int16_t x, y;
    data->state = LV_INDEV_STATE_RELEASED;
    if (ft3168_read_touch(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PRESSED;
    }
}

/**
 * LVGL invalidation area rounder.
 *
 * SH8601 / CO5300 require that CASET/PASET windows are aligned to even
 * boundaries.  Odd-aligned areas cause garbled/shifted rendering.
 */
static void lvgl_invalidate_area_cb(lv_event_t *e)
{
    lv_area_t *area = (lv_area_t *)lv_event_get_param(e);
    area->x1 = area->x1 & ~1;   // round down to even
    area->y1 = area->y1 & ~1;
    area->x2 = area->x2 |  1;   // round up to odd (end of even pair)
    area->y2 = area->y2 |  1;
}

/**
 * esp_timer callback that increments the LVGL tick counter.
 */
static void IRAM_ATTR lvgl_tick_cb(void * /*arg*/)
{
    lv_tick_inc(LVGL_TICK_MS);
}

/**
 * FreeRTOS task that drives lv_timer_handler().
 *
 * Always holds the LVGL mutex while calling into LVGL, and releases
 * it between iterations so other tasks can call LVGL APIs safely.
 */
static void lvgl_task(void * /*arg*/)
{
    for (;;) {
        xSemaphoreTakeRecursive(s_lvgl_mux, portMAX_DELAY);
        uint32_t delay_ms = lv_timer_handler();
        xSemaphoreGiveRecursive(s_lvgl_mux);

        // Clamp delay to a sensible range
        if (delay_ms > 500) delay_ms = 500;
        if (delay_ms < 1)   delay_ms = 1;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// ---------------------------------------------------------------------------
// Helpers for calling LVGL from outside the LVGL task
// ---------------------------------------------------------------------------
static bool lvgl_lock(int timeout_ms = -1)
{
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY
                                        : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mux, ticks) == pdTRUE;
}

static void lvgl_unlock()
{
    xSemaphoreGiveRecursive(s_lvgl_mux);
}


// ---------------------------------------------------------------------------
// Dashboard UI (layout + theme defined in dash_ui.h)
// ---------------------------------------------------------------------------
#include "gauges/dash_ui.h"


// ---------------------------------------------------------------------------
// Speed arc smooth animation
// Runs inside lv_timer_handler() under the LVGL mutex every 16 ms.
// Lerps the displayed arc value toward s_speed_target (rounded to nearest 10).
// ---------------------------------------------------------------------------
static void speed_smooth_cb(lv_timer_t * /*t*/)
{
    static float smooth = 0.0f;
    float target = (float)s_speed_target;
    smooth += (target - smooth) * 0.15f;
    int displayed = (int)(smooth + 0.5f);
    lv_arc_set_value(s_speed_arc, displayed);
    lv_label_set_text_fmt(s_speed_label, "%d", displayed);
}
// ---------------------------------------------------------------------------
// ESP-NOW cluster network -- screen unit
// ---------------------------------------------------------------------------
#define PAIR_REQ_INTERVAL_MS  2000   /* ms between pair requests when unconnected */
#define CONN_TIMEOUT_MS       5000   /* ms of silence before re-pairing           */

static bool     s_connected     = false;
static uint8_t  s_master_mac[6] = {};
static uint32_t s_last_gauge_ms = 0;
static uint32_t s_last_pair_req = 0;

static const uint8_t BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static void send_pair_req(void)
{
    cluster_pair_req_t req = {};
    req.magic     = CLUSTER_MAGIC;
    req.pkt_type  = PKT_PAIR_REQ;
    req.unit_type = UNIT_SCREEN;
    snprintf(req.unit_id, sizeof(req.unit_id), "SCR-001");
    esp_now_send(BROADCAST_MAC, (const uint8_t *)&req, sizeof(req));
    Serial.println("[SCREEN] PAIR_REQ ->");
}

static void espnow_recv_cb(const esp_now_recv_info_t *info,
                            const uint8_t *data, int len)
{
    if (len < 3) return;
    uint16_t magic = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    if (magic != CLUSTER_MAGIC) return;
    uint8_t pkt_type = data[2];

    if (pkt_type == PKT_PAIR_ACK && !s_connected) {
        const cluster_pair_ack_t *ack = (const cluster_pair_ack_t *)data;
        if (!ack->accepted) return;
        memcpy(s_master_mac, info->src_addr, 6);
        s_connected     = true;
        s_last_gauge_ms = millis();   /* prevent immediate timeout */
        Serial.printf("[SCREEN] Paired with master %02X:%02X:%02X:%02X:%02X:%02X\n",
            s_master_mac[0], s_master_mac[1], s_master_mac[2],
            s_master_mac[3], s_master_mac[4], s_master_mac[5]);
        
        return;
    }

    if (pkt_type == PKT_DYNAMICS) {
        const cluster_dynamics_pkt_t *pkt = (const cluster_dynamics_pkt_t *)data;
        s_last_gauge_ms = millis();
        s_speed_target = (((int)pkt->speed + 5) / 10) * 10;
        if (lvgl_lock(10)) {
            lv_label_set_text_fmt(s_rpm_label, "%d", (int)pkt->rpm);
            lvgl_unlock();
        }
        return;
    }

    if (pkt_type == PKT_STATUS) {
        const cluster_status_pkt_t *pkt = (const cluster_status_pkt_t *)data;
        if (lvgl_lock(10)) {
            lv_color_t fc = (pkt->fuel_pct > 25) ? lv_color_hex(0x44cc44) :
                            (pkt->fuel_pct > 10) ? lv_color_hex(0xffaa00) :
                                                   lv_color_hex(0xff3322);
            lv_obj_set_style_arc_color(s_fuel_arc, fc, LV_PART_INDICATOR);
            lv_arc_set_value(s_fuel_arc, pkt->fuel_pct);
            lvgl_unlock();
        }
        return;
    }

    /* PKT_GEAR -- reserved for future gear indicator widget */
}

static void espnow_init(void)
{
    WiFi.mode(WIFI_STA);
    // NOTE: do NOT call WiFi.disconnect() -- it resets the channel to 0
    Serial.printf("[SCREEN] MAC: %s\n", WiFi.macAddress().c_str());
    if (esp_now_init() != ESP_OK) {
        Serial.println("[SCREEN] ERROR: esp_now_init() failed");
        return;
    }
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_now_register_recv_cb(espnow_recv_cb);
    uint8_t _ch; wifi_second_chan_t _sch; esp_wifi_get_channel(&_ch, &_sch);
    Serial.printf("[SCREEN] WiFi channel: %d\n", _ch);

    /* Register broadcast peer -- required to send PAIR_REQ */
    esp_now_peer_info_t bcast = {};
    memset(bcast.peer_addr, 0xFF, 6);
    bcast.channel = 0;
    bcast.encrypt = false;
    esp_now_add_peer(&bcast);

    Serial.println("[SCREEN] ESP-NOW ready -- broadcasting pairing request");
}


// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    Serial.println("\n\n=== Waveshare ESP32-S3-Touch-AMOLED-1.43 ===");

    // ---- 1. Enable display power (GPIO 42 must be HIGH before init) ----
    gpio_config_t en_cfg = {
        .pin_bit_mask = (1ULL << LCD_EN_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&en_cfg);
    gpio_set_level(static_cast<gpio_num_t>(LCD_EN_GPIO), 1);
    delay(100);   // Allow power supply to stabilise

    // ---- 2. Initialise board (ESP32_Display_Panel) ----
    Serial.println("Initializing ESP32_Display_Panel board...");
    s_board = new Board();
    s_board->init();
    assert(s_board->begin() && "Board init failed -- check custom board config");

    LCD   *lcd   = s_board->getLCD();
    // Touch is NOT managed by Board (see esp_panel_board_custom_conf.h);
    // initialised separately below.
    Serial.printf("LCD:   %dx%d\n", lcd->getFrameWidth(), lcd->getFrameHeight());

    // FT3168 needs settle time after the LCD reset pulse on GPIO 21
    delay(300);

    // ---- 3. Initialise LVGL ----
    lv_init();

    // Allocate two render buffers from DMA-capable internal SRAM.
    // DO NOT use PSRAM here: QSPI DMA at 40 MHz requires fast SRAM.
    uint8_t *buf1 = static_cast<uint8_t *>(
        heap_caps_malloc(LVGL_BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    uint8_t *buf2 = static_cast<uint8_t *>(
        heap_caps_malloc(LVGL_BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    assert(buf1 && buf2 && "LVGL buffer allocation failed");
    Serial.printf("LVGL buffers: 2 x %u bytes (DMA SRAM)\n", LVGL_BUF_BYTES);

    // ---- 4. Create LVGL display ----
    s_display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    // SH8601 sends RGB565 big-endian over QSPI; byte swap is enabled via
    // LV_COLOR_16_SWAP in lv_conf.h. LV_COLOR_FORMAT_RGB565 is the correct
    // constant in LVGL 9.2.2 (SWAPPED variant does not exist in this release).
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);
    lv_display_set_buffers(s_display, buf1, buf2,
                           LVGL_BUF_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(s_display, lcd);

    // Register the coordinate rounder: SH8601/CO5300 requires even CASET/PASET values
    lv_display_add_event_cb(s_display,
                            lvgl_invalidate_area_cb,
                            LV_EVENT_INVALIDATE_AREA, NULL);

    // ---- 5. Create direct FT5x06 touch driver (I2C) ----
    {
        using namespace esp_panel::drivers;
        BusI2C::Config i2c_cfg;
        i2c_cfg.host_id = 0;  // I2C_NUM_0
        BusI2C::HostPartialConfig host;
        host.scl_io_num    = TOUCH_SCL_GPIO;
        host.sda_io_num    = TOUCH_SDA_GPIO;
        host.sda_pullup_en = true;
        host.scl_pullup_en = true;
        host.clk_speed     = 100 * 1000;  // 100 kHz -- reliable over PCB traces
        i2c_cfg.host = host;
        i2c_cfg.control_panel = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        i2c_cfg.control_panel.dev_addr = TOUCH_I2C_ADDR;
        Touch::Config touch_cfg;
        touch_cfg.device = Touch::DevicePartialConfig{
            .x_max          = LCD_WIDTH  - 1,
            .y_max          = LCD_HEIGHT - 1,
            .rst_gpio_num   = -1,
            .int_gpio_num   = -1,
        };
        s_touch = new TouchFT5x06(i2c_cfg, touch_cfg);
        bool touch_ok = false;
        if (s_touch->init()) {
            // FT3168 may be in monitor mode (low-power) from a previous boot.
            // On FT5x06-family devices any I2C START + address match wakes the
            // device.  Send a harmless dummy write here (before begin() tries to
            // write config registers) so the device is guaranteed to be awake.
            uint8_t wake_reg = 0x00; // DEVICE_MODE = 0x00, value 0 = normal mode
            i2c_master_write_to_device(I2C_NUM_0, TOUCH_I2C_ADDR,
                                       &wake_reg, 1, pdMS_TO_TICKS(50));
            delay(10);   // FT3168 needs ~5 ms to leave monitor mode

            if (s_touch->begin()) {
                // FT5x06 driver wrote TIME_ENTER_MONITOR = 2 (sleep after 2s).
                // With no INT pin wired we cannot wake the device, so set the
                // register to 0 to disable monitor mode permanently.
                uint8_t dis_mon[2] = { 0x87 /* TIME_ENTER_MONITOR */, 0x00 };
                esp_err_t wr = i2c_master_write_to_device(
                    I2C_NUM_0, TOUCH_I2C_ADDR,
                    dis_mon, sizeof(dis_mon), pdMS_TO_TICKS(100));
                Serial.printf("Touch: FT5x06 OK (monitor-mode disable: %s)\n",
                              esp_err_to_name(wr));
                touch_ok = true;
            } else {
                Serial.println("Touch: FT5x06 begin FAILED");
            }
        } else {
            Serial.println("Touch: FT5x06 init FAILED");
        }
        if (!touch_ok) {
            delete s_touch;
            s_touch = nullptr;
        }
    }

    // ---- 6. Register touch input device with LVGL ----
    if (s_touch) {
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, lvgl_touch_cb);
        lv_indev_set_user_data(indev, static_cast<void *>(s_touch));
        lv_indev_set_display(indev, s_display);
    }

    // ---- 7. Start LVGL tick timer ----
    const esp_timer_create_args_t tick_args = {
        .callback             = lvgl_tick_cb,
        .arg                  = nullptr,
        .dispatch_method      = ESP_TIMER_TASK,
        .name                 = "lvgl_tick",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t tick_timer = nullptr;
    esp_timer_create(&tick_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, LVGL_TICK_MS * 1000ULL);

    // ---- 8. Create LVGL mutex and task ----
    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(s_lvgl_mux);
    xTaskCreatePinnedToCore(lvgl_task, "lvgl",
                            LVGL_TASK_STACK, nullptr,
                            LVGL_TASK_PRIORITY, nullptr,
                            ARDUINO_RUNNING_CORE);

    // ---- 9. Build the dashboard UI with default theme ----
    lvgl_lock();
    g_theme = CLUSTER_THEMES[0];   /* default: Nightfall; master overrides on pair */

    create_dash_ui();
    lv_timer_create(speed_smooth_cb, 16, NULL);
    lvgl_unlock();

    // ---- 10. Init ESP-NOW and begin pairing with master unit ----
    espnow_init();
    send_pair_req();
    s_last_pair_req = millis();

    Serial.println("[SCREEN] Setup complete -- searching for master unit");
}


// ---------------------------------------------------------------------------
// loop() -- pairing heartbeat (gauge display driven by ESP-NOW recv callback)
// ---------------------------------------------------------------------------
void loop()
{
    static uint32_t fps_ms = 0;
    uint32_t now = millis();

    if (now - fps_ms >= 1000) {
        uint32_t frames = s_frame_count;
        s_frame_count   = 0;
        fps_ms          = now;
        Serial.printf("[FPS] %u\n", frames);
    }

    if (!s_connected) {
        /* Broadcast PAIR_REQ periodically until a master responds */
        if (now - s_last_pair_req >= PAIR_REQ_INTERVAL_MS) {
            s_last_pair_req = now;
            send_pair_req();
        }
    } else {
        /* Re-enter pairing if master has gone silent */
        if (now - s_last_gauge_ms > CONN_TIMEOUT_MS) {
            Serial.println("[SCREEN] Master timeout -- re-pairing");
            s_connected = false;
            esp_now_del_peer(s_master_mac);
            /* Re-register broadcast peer for sending PAIR_REQ */
            esp_now_peer_info_t bcast = {};
            memset(bcast.peer_addr, 0xFF, 6);
            bcast.channel = 0;
            bcast.encrypt = false;
            esp_now_add_peer(&bcast);
            s_last_pair_req = 0;   /* trigger immediate re-pair attempt */
        }
    }

    delay(1);
}
