/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/cad.h"
#ifndef EN_VERSION
#define EN_VERSION "1.0"
#endif
int project_load(void);
void project_apply(void);
int project_save(void);
int project_snapshot(void);
void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a,const char *b);
