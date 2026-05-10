#pragma once
/*
 * core/themes/styles.h  —  Theme palette definitions
 *
 * Defines cluster_theme_t and all built-in colour palettes.
 * Shared across all cluster units (screen, master, sensor).
 *
 * Include in EVERY unit that reads theme data.
 * Define CLUSTER_DEFINE_THEMES in exactly ONE translation unit per binary
 * before including this file to instantiate the CLUSTER_THEMES[] array.
 *
 * Field order in cluster_theme_t:
 *   { "name", pri_color, sec_color, bg_color, inactive_color,
 *             warn_color, danger_color, text_color, subtext_color }
 * All colour fields are 0x00RRGGBB uint32.
 */

#include <stdint.h>
#include <string.h>  /* memcpy (used by packet structs that embed this type) */

/* ── Theme type ─────────────────────────────────────────────────────────── */
typedef struct {
    char     name[16];          /* human-readable label, e.g. "Nightfall"   */
    uint32_t pri_color;         /* primary arc / indicator fill              */
    uint32_t sec_color;         /* secondary accent  (fuel arc default fill) */
    uint32_t bg_color;          /* screen background                         */
    uint32_t inactive_color;    /* arc track / dim background elements       */
    uint32_t warn_color;        /* warning level  (e.g. low-fuel amber)      */
    uint32_t danger_color;      /* alarm level    (e.g. critical red)        */
    uint32_t text_color;        /* main readout text                         */
    uint32_t subtext_color;     /* unit labels, tick marks                   */
} cluster_theme_t;

/* ── Built-in palettes ──────────────────────────────────────────────────── */
#ifdef CLUSTER_DEFINE_THEMES
static const cluster_theme_t CLUSTER_THEMES[] = {

    /* Active theme — uncomment exactly ONE block below */

    /* 0 — Nightfall: cyan on deep navy */
    { "Nightfall",  0x00cfff, 0x00aadd, 0x050505, 0x0d2e4a,
                    0xffaa00, 0xff3322, 0xffffff, 0x4a6070 },

//  /* 1 — Rally: orange-red on dark amber-brown */
//  { "Rally",      0xff4400, 0xcc2200, 0x050505, 0x3a1200,
//                  0xffee00, 0xff0000, 0xffffff, 0x604030 },

//  /* 2 — Ghost: silver on dark charcoal */
//  { "Ghost",      0xcccccc, 0x888888, 0x050505, 0x282828,
//                  0xffcc00, 0xff2200, 0xffffff, 0x505050 },

//  /* 3 — Crimson: red on dark purple-red */
//  { "Crimson",    0xff2255, 0xcc0033, 0x050505, 0x3a0028,
//                  0xff8800, 0xff0000, 0xffffff, 0x603040 },
};
#define CLUSTER_THEME_COUNT  ((int)(sizeof(CLUSTER_THEMES) / sizeof(CLUSTER_THEMES[0])))
#endif /* CLUSTER_DEFINE_THEMES */
