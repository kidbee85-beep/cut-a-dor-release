/* SPDX-License-Identifier: GPL-3.0-only
 * Panel layout adapted from Felucca, Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hugelton Instruments. */
#pragma once
#include "plat.h"
#define PANEL_MAGIC 0x50414E35u          /* "PAN5": Felucca's layout, so its calibration stays valid */

typedef struct {
    uint32_t magic;
    uint8_t btn[NB];             /* matrix button id (0..13) per label */
    uint8_t enc[NE];             /* matrix encoder (0..6) per role */
    int8_t dir[NE];              /* +1 / -1 so that clockwise is + */
} panel_t;

static const panel_t PANEL_DEFAULT = {
    PANEL_MAGIC,
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1},
    {0, 1, 6, 2, 3, 4, 5},
    {1, 1, 1, 1, 1, 1, 1},
};

static int panel_valid(const panel_t *p)
{
    uint32_t buttons = 0, encoders = 0;
    if (p->magic != PANEL_MAGIC) return 0;
    for (unsigned i = 0; i < NB; i++) {
        if (p->btn[i] >= NB || (buttons & (1u << p->btn[i]))) return 0;
        buttons |= 1u << p->btn[i];
    }
    for (unsigned i = 0; i < NE; i++) {
        if (p->enc[i] >= NE || (encoders & (1u << p->enc[i])) ||
            (p->dir[i] != 1 && p->dir[i] != -1)) return 0;
        encoders |= 1u << p->enc[i];
    }
    return 1;
}

