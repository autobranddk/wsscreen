#pragma once
/*
 * core/cluster_types.h  —  ESP-NOW protocol constants and packet definitions
 *
 * Include in EVERY cluster unit (master, screen, sensor).
 * Written in C99-compatible style so it works in both C++ (Arduino) and
 * C (the PC simulator's main.c).
 *
 * Theme colour data (cluster_theme_t, CLUSTER_THEMES[]) lives in
 * core/themes/styles.h, which is included automatically below.
 * Define CLUSTER_DEFINE_THEMES in exactly ONE translation unit per binary
 * before including this header to instantiate the CLUSTER_THEMES[] array.
 */

#include "themes/styles.h"   /* cluster_theme_t, CLUSTER_THEMES[], CLUSTER_THEME_COUNT */

/* ── Protocol constants ─────────────────────────────────────────────────── */
#define CLUSTER_MAGIC   0xCA1B   /* 16-bit stamp in every packet header     */

/* Unit types (cluster_pair_req_t.unit_type) */
#define UNIT_SCREEN   0
#define UNIT_SENSOR   1

/* Packet type codes — first byte after the 2-byte magic */
#define PKT_DYNAMICS     0x01   /* cluster_dynamics_pkt_t — ~60 Hz: rpm, speed */
#define PKT_GEAR         0x02   /* cluster_gear_pkt_t     — ~10 Hz: gear       */
#define PKT_STATUS       0x03   /* cluster_status_pkt_t   —  ~5 Hz: fuel, temp, flags */
#define PKT_THEME        0x04   /* cluster_theme_pkt_t    — sent on change     */
#define PKT_PAIR_REQ     0x10   /* cluster_pair_req_t     — screen → master    */
#define PKT_PAIR_ACK     0x11   /* cluster_pair_ack_t     — master → screen    */

/* Status flags (cluster_status_pkt_t.flags bitmask) */
#define FLAG_CHECK_ENGINE  0x01
#define FLAG_LOW_FUEL      0x02
#define FLAG_HANDBRAKE     0x04

/* ── Packets (packed to avoid padding across MCU architectures) ─────────── */

/* High-frequency dynamics — ~60 Hz */
typedef struct __attribute__((packed)) {
    uint16_t magic;         /* CLUSTER_MAGIC                                */
    uint8_t  pkt_type;      /* PKT_DYNAMICS                                 */
    uint16_t rpm;           /* 0 – 8 000                                    */
    uint8_t  speed;         /* 0 – 255 km/h                                 */
} cluster_dynamics_pkt_t;

/* Gear indicator — ~10 Hz */
typedef struct __attribute__((packed)) {
    uint16_t magic;         /* CLUSTER_MAGIC                                */
    uint8_t  pkt_type;      /* PKT_GEAR                                     */
    uint8_t  gear;          /* 0=N  1-8=forward  9=R                        */
} cluster_gear_pkt_t;

/* Slow status — ~5 Hz: fuel, temperature, warning flags */
typedef struct __attribute__((packed)) {
    uint16_t magic;         /* CLUSTER_MAGIC                                */
    uint8_t  pkt_type;      /* PKT_STATUS                                   */
    uint8_t  fuel_pct;      /* 0 – 100 %                                    */
    int8_t   coolant_c;     /* -40 … +120 °C                                */
    uint8_t  flags;         /* FLAG_CHECK_ENGINE | FLAG_LOW_FUEL | ...      */
} cluster_status_pkt_t;

/* Theme packet — sent on pairing ACK or when theme changes at runtime */
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  pkt_type;      /* PKT_THEME                                    */
    cluster_theme_t theme;
} cluster_theme_pkt_t;

/* Pairing request — screen → master, addressed to broadcast MAC */
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  pkt_type;      /* PKT_PAIR_REQ                                 */
    uint8_t  unit_type;     /* UNIT_SCREEN                                  */
    char     unit_id[12];   /* e.g. "SCR-001"                               */
} cluster_pair_req_t;

/* Pairing acknowledgement — master → screen, unicast */
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  pkt_type;      /* PKT_PAIR_ACK                                 */
    uint8_t  accepted;      /* 1 = accepted, 0 = rejected                   */
    cluster_theme_t active_theme;
} cluster_pair_ack_t;
